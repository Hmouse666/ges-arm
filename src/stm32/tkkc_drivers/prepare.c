//#include "main.h"

//uint8_t txData[] = "Hello World\n";

//void app_main(void)
//{
//    while(1)
//    {
//        HAL_UART_Transmit(&huart1, txData, sizeof(txData), HAL_MAX_DELAY);
//        HAL_Delay(1000);
//    }
//}


//#include "main.h"

//uint8_t rxData;
//uint8_t txData;
//uint8_t send_flag = 0;

//void app_main(void)
//{
//    HAL_UART_Receive_IT(&huart1, &rxData, sizeof(rxData));

//    while(1)
//    {
//        if(send_flag == 1)
//        {
//            send_flag = 0;
//            HAL_UART_Transmit(&huart1, &txData, sizeof(txData), HAL_MAX_DELAY);
//        }
//    }
//}

//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
//{
//    if(huart == &huart1)
//    {
//        txData = rxData;
//        send_flag = 1;

//        HAL_UART_Receive_IT(&huart1, &rxData, sizeof(rxData));
//    }
//}
