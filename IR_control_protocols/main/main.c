#include <stdio.h>

#include "esp32-rmt-ir.h"

#define IR_TX_GPIO_NUM       18
#define IR_RX_GPIO_NUM       19


void irReceived(irproto brand, uint32_t code, size_t len, rmt_symbol_word_t *item){
	if( code ){
		printf("IR %s, code: %lx, bits: %d\n",  proto[brand].name, code, len);
	}
	
	if(true){//debug
		printf("Rx%d: ", len);							
		for (uint8_t i=0; i < len ; i++ ) {
			int d0 = item[i].duration0; if(!item[i].level0){d0 *= -1;}
			int d1 = item[i].duration1; if(!item[i].level1){d1 *= -1;}
			printf("%d,%d ", d0, d1);
		}								
		printf("\n");
	}
  
}

void app_main(void)
{

    irRxPin = 19;
	irTxPin = 18;
	xTaskCreatePinnedToCore(recvIR, "recvIR", 2048*4, NULL, 10, NULL, 1);

    while (1)
    {
        /*
        sendIR(NEC, 0xC1AAFC03, 32, 1, 1); //protocol, code, bits, bursts, repeats
        vTaskDelay(pdMS_TO_TICKS(1000));
        sendIR(SONY, 0x3e108, 20, 1, 1); //protocol, code, bits, bursts, repeats
        
        */
       vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}