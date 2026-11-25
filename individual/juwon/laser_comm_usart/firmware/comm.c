/*
 * comm.c - USART2 인터럽트 기반 송수신 (StdPeriph)
 *
 * 필요한 StdPeriph 소스: stm32f4xx_usart.c, stm32f4xx_gpio.c, stm32f4xx_rcc.c, misc.c
 * (수업 실습 1의 프로젝트 파일 구성과 같음)
 */
#include "stm32f4xx.h"
#include "comm.h"

/* ---------------- 링버퍼 크기 (2의 거듭제곱) ---------------- */
#define TX_BUF_SIZE  128u   /* 27B 프레임 4개 이상 */
#define RX_BUF_SIZE  32u
#define TX_MASK      (TX_BUF_SIZE - 1u)
#define RX_MASK      (RX_BUF_SIZE - 1u)

static volatile uint8_t  tx_buf[TX_BUF_SIZE];
static volatile uint16_t tx_head = 0;   /* main이 씀 */
static volatile uint16_t tx_tail = 0;   /* ISR이 씀 */

static volatile uint8_t  rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;   /* ISR이 씀 */
static volatile uint16_t rx_tail = 0;   /* main이 씀 */

static volatile ProtoStatus g_status;   /* 다음 프레임에 실릴 내용 */
static volatile uint8_t  g_send_flag = 0;
static volatile uint32_t g_tx_drop = 0;
static volatile uint32_t g_rx_drop = 0;

/* ---------------- 초기화: 수업 USART2_Configuration + 인터럽트 ---------------- */
void Comm_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    /* 수신 인터럽트 ON. 송신 인터럽트(TXE)는 보낼 데이터가 생길 때만 켬 */
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    /* 우선순위: TIM2(0)·ADC/EXTI보다 낮게. 그룹은 수업과 같은 PriorityGroup_1
     * (팀 전체가 같은 그룹을 써야 함 - 서로 다른 그룹으로 여러 번 부르면 마지막 것만 적용) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    NVIC_InitStructure.NVIC_IRQChannel                   = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

/* ---------------- 다른 모듈이 상태를 넣는 함수 ---------------- */
void Comm_SetState(uint8_t state)
{
    g_status.state = state;
}

void Comm_ReportHit(uint8_t zone, uint16_t reaction_ms)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    g_status.zone        = zone;
    g_status.reaction_ms = reaction_ms;
    g_status.hit_cnt++;
    __set_PRIMASK(pm);
}

void Comm_SetAdc(const uint16_t adc[PROTO_NUM_CH])
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    for (uint32_t i = 0; i < PROTO_NUM_CH; i++) g_status.adc[i] = adc[i];
    __set_PRIMASK(pm);
}

/* TIM2 ISR에서 10ms마다 호출: 여기서는 플래그만 세움 (ISR 안에서 송신 X) */
void Comm_Tick10ms(void)
{
    g_send_flag = 1;
}

/* ---------------- 송신 ---------------- */
static uint16_t tx_free(void)
{
    return (uint16_t)(TX_BUF_SIZE - 1u - ((tx_head - tx_tail) & TX_MASK));
}

static void tx_enqueue(const uint8_t *p, uint32_t n)
{
    if (tx_free() < n) {          /* 프레임 단위로 통째로 버림 (반쪽 프레임 금지) */
        g_tx_drop++;
        return;
    }
    for (uint32_t i = 0; i < n; i++) {
        tx_buf[tx_head] = p[i];
        tx_head = (uint16_t)((tx_head + 1u) & TX_MASK);
    }
    USART_ITConfig(USART2, USART_IT_TXE, ENABLE);   /* ISR이 하나씩 꺼내 보냄 */
}

/* main while(1)에서 계속 호출 */
void Comm_Process(void)
{
    if (!g_send_flag) return;
    g_send_flag = 0;

    ProtoStatus snap;
    uint8_t frame[PROTO_FRAME_LEN];

    uint32_t pm = __get_PRIMASK();
    __disable_irq();                       /* 프레임 하나가 같은 시점의 값이 되도록 복사 */
    g_status.seq++;
    snap.seq         = g_status.seq;
    snap.state       = g_status.state;
    snap.hit_cnt     = g_status.hit_cnt;
    snap.zone        = g_status.zone;
    snap.reaction_ms = g_status.reaction_ms;
    for (uint32_t i = 0; i < PROTO_NUM_CH; i++) snap.adc[i] = g_status.adc[i];
    __set_PRIMASK(pm);

    uint32_t len = Proto_BuildFrame(&snap, frame);
    tx_enqueue(frame, len);
}

/* ---------------- 수신 ---------------- */
int Comm_GetCommand(void)
{
    if (rx_tail == rx_head) return -1;
    uint8_t c = rx_buf[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & RX_MASK);
    return c;
}

/* ---------------- 인터럽트 ---------------- */
void USART2_IRQHandler(void)
{
    /* 수신: RXNE 또는 오버런. SR 읽고 DR 읽으면 ORE도 같이 지워짐 */
    if (USART2->SR & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t d = (uint8_t)USART_ReceiveData(USART2);
        uint16_t next = (uint16_t)((rx_head + 1u) & RX_MASK);
        if (next != rx_tail) {
            rx_buf[rx_head] = d;
            rx_head = next;
        } else {
            g_rx_drop++;
        }
    }

    /* 송신: 데이터 레지스터가 비면 다음 바이트 */
    if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
        if (tx_tail != tx_head) {
            USART_SendData(USART2, tx_buf[tx_tail]);
            tx_tail = (uint16_t)((tx_tail + 1u) & TX_MASK);
        } else {
            USART_ITConfig(USART2, USART_IT_TXE, DISABLE);   /* 다 보냈으면 끔 */
        }
    }
}

uint32_t Comm_TxDropCount(void) { return g_tx_drop; }
uint32_t Comm_RxDropCount(void) { return g_rx_drop; }
