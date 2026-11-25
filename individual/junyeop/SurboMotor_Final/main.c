int main(void)
{
    LED_Init();
    Servo_Init();

    Servo_SetAngle(90);

    while (1)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_5);
        DelayMs(500);

        GPIO_ResetBits(GPIOA, GPIO_Pin_5);
        DelayMs(500);
    }
}