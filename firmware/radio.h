// kgsws' nRF24L01 library

extern uint8_t nrf_status;

void NRF_Init();
uint8_t NRF_ReadStatus();
void NRF_Command(uint8_t cmd);
uint8_t NRF_CommandRead(uint8_t cmd);
void NRF_GetRegister(uint8_t reg, __xdata uint8_t *buff, uint8_t length);
uint8_t NRF_GetReg(uint8_t reg);
void NRF_SetRegister(uint8_t reg, __xdata uint8_t *buff, uint8_t length);
void NRF_SetReg(uint8_t reg, uint8_t out);
void NRF_ReadData(__xdata uint8_t *buf, uint8_t count);
void NRF_WriteData(__xdata uint8_t *buf, uint8_t count);
void NRF_ClearIRQ();
//void NRF_Scan(uint8_t *buff);
