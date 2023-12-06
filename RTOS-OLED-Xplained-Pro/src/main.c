#include "conf_board.h"
#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/** IOS **/
#define LED_1_PIO PIOA
#define LED_1_PIO_ID ID_PIOA
#define LED_1_IDX 0
#define LED_1_IDX_MASK (1 << LED_1_IDX)

#define LED_2_PIO PIOC
#define LED_2_PIO_ID ID_PIOC
#define LED_2_IDX 30
#define LED_2_IDX_MASK (1 << LED_2_IDX)

#define LED_3_PIO PIOB
#define LED_3_PIO_ID ID_PIOB
#define LED_3_IDX 2
#define LED_3_IDX_MASK (1 << LED_3_IDX)

#define LED_R_PIO PIOD
#define LED_R_PIO_ID ID_PIOD
#define LED_R_IDX 27
#define LED_R_IDX_MASK (1 << LED_R_IDX)

#define LED_G_PIO PIOA
#define LED_G_PIO_ID ID_PIOA
#define LED_G_IDX 21
#define LED_G_IDX_MASK (1 << LED_G_IDX)

#define LED_B_PIO PIOA
#define LED_B_PIO_ID ID_PIOA
#define LED_B_IDX 6
#define LED_B_IDX_MASK (1 << LED_B_IDX)

#define BUT_1_PIO PIOD
#define BUT_1_PIO_ID ID_PIOD
#define BUT_1_IDX 28
#define BUT_1_IDX_MASK (1u << BUT_1_IDX)

#define BUT_2_PIO PIOC
#define BUT_2_PIO_ID ID_PIOC
#define BUT_2_IDX 31
#define BUT_2_IDX_MASK (1u << BUT_2_IDX)

#define BUT_3_PIO PIOA
#define BUT_3_PIO_ID ID_PIOA
#define BUT_3_IDX 19
#define BUT_3_IDX_MASK (1u << BUT_3_IDX)

#define AFEC_POT AFEC0
#define AFEC_POT_ID ID_AFEC0
#define AFEC_POT_CHANNEL 0 // Canal do pino PD30

/** RTOS  */
#define TASK_OLED_STACK_SIZE (1024 * 6 / sizeof(portSTACK_TYPE))
#define TASK_OLED_STACK_PRIORITY (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void io_init(void);
static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel,
afec_callback_t callback);
TimerHandle_t xTimer;

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName) {
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;) {
	}
}

extern void vApplicationIdleHook(void) {}

extern void vApplicationTickHook(void) {}

extern void vApplicationMallocFailedHook(void) {
	configASSERT((volatile void *)NULL);
}

QueueHandle_t xQueueBtn;
QueueHandle_t xQueueAFEC;
QueueHandle_t xQueueR;
QueueHandle_t xQueueG;
QueueHandle_t xQueueB;
/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/
typedef struct {
	uint id;
	uint status;
} struBut;

void but1_callback(void) {
	// printf("1 \n");
	if (pio_get(BUT_1_PIO, PIO_INPUT, BUT_1_IDX_MASK)) {
		// PINO == 1 --> Borda de subida
		struBut data;
		data.id = 1;
		data.status = 0;
		xQueueSendFromISR(xQueueBtn, (void *)&data, 10);
		} else {
		// PINO == 0 --> Borda de descida
		struBut data;
		data.id = 1;
		data.status = 1;
		xQueueSendFromISR(xQueueBtn, (void *)&data, 10);
	}

}

void but2_callback(void) {
	if (pio_get(BUT_2_PIO, PIO_INPUT, BUT_2_IDX_MASK)) {
		// PINO == 1 --> Borda de subida
		struBut data;
		data.id = 2;
		data.status = 0;
		xQueueSendFromISR(xQueueBtn, (void *)&data, 10);
		} else {
		// PINO == 0 --> Borda de descida
		struBut data;
		data.id = 2;
		data.status = 1;
		xQueueSendFromISR(xQueueBtn, (void *)&data, 10);
	}
}

void but3_callback(void) {
	//  printf("3 \n");
	if (pio_get(BUT_3_PIO, PIO_INPUT, BUT_3_IDX_MASK)) {
		// PINO == 1 --> Borda de subida
		struBut data;
		data.id = 3;
		data.status = 0;
		xQueueSendFromISR(xQueueBtn, (void *)&data, 10);
		} else {
		// PINO == 0 --> Borda de descida
		struBut data;
		data.id = 3;
		data.status = 1;
		xQueueSendFromISR(xQueueBtn, (void *)&data, 10);
	}
}

static void AFEC_pot_Callback(void){
	uint adc;
	adc = afec_channel_get_value(AFEC_POT, AFEC_POT_CHANNEL);
	BaseType_t xHigherPriorityTaskWoken = pdTRUE;
	xQueueSendFromISR(xQueueAFEC, &adc, &xHigherPriorityTaskWoken);
}

void vTimerCallback(TimerHandle_t xTimer) {
	/* Selecina canal e inicializa convers?o */
	afec_channel_enable(AFEC_POT, AFEC_POT_CHANNEL);
	afec_start_software_conversion(AFEC_POT);
}
/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_oled(void *pvParameters) {
	gfx_mono_ssd1306_init();

	for (;;) {
		gfx_mono_draw_filled_circle(12, 12, 4, GFX_PIXEL_XOR,
		GFX_QUADRANT0 | GFX_QUADRANT1 | GFX_QUADRANT2 |
		GFX_QUADRANT3);
		vTaskDelay(200);
	}
}

static void task_color (void *pvParameters){
	struBut but;
	uint af;
	config_AFEC_pot(AFEC_POT, AFEC_POT_ID, AFEC_POT_CHANNEL, AFEC_pot_Callback);
	xTimer = xTimerCreate(/* Just a text name, not used by the RTOS
	kernel. */
	"Timer",
	/* The timer period in ticks, must be
	greater than 0. */
	100,
	/* The timers will auto-reload themselves
	when they expire. */
	pdTRUE,
	/* The ID is used to store a count of the
	number of times the timer has expired, which
	is initialised to 0. */
	(void *)0,
	/* Timer callback */
	vTimerCallback);
	xTimerStart(xTimer, 0);
	int but1 =0;
	int but2 =0;
	int but3=0;
	int valor=0;

	for(;;){
		if( xQueueReceive( xQueueAFEC, &af, ( TickType_t ) 0 )){
			valor = (int) (af*20.0/4095.0);
		}
		if( xQueueReceive( xQueueBtn, &but, ( TickType_t ) 0 )){
			if (but.id == 1){
				if (but.status == 1){
					but1 = 1;
					pio_clear(LED_1_PIO, LED_1_IDX_MASK);
				//	delay_ms(100);
				}
				if (but.status == 0){
					but1 = 0;
					pio_set(LED_1_PIO, LED_1_IDX_MASK);
					
				//	delay_ms(100);
				}
			}
			if (but.id == 2){
				if (but.status == 1){
					but2 = 1;
					pio_clear(LED_2_PIO, LED_2_IDX_MASK);
					//delay_ms(100);
				}
				if (but.status == 0){
					but2 = 0;
					pio_set(LED_2_PIO, LED_2_IDX_MASK);
					
					//delay_ms(100);
				}
			}
			
			if (but.id == 3){
				if (but.status == 1){
					but3 = 1;
					pio_clear(LED_3_PIO, LED_3_IDX_MASK);
					//delay_ms(100);
				}
				if (but.status == 0){
					but3 = 0;
					pio_set(LED_3_PIO, LED_3_IDX_MASK);
					//delay_ms(100);
				}
			}
		}
		if (but1 == 1 ){
			
			xQueueSend(xQueueR, &valor, 0);
			//delay_ms(150);
			
		}
		if (but2 == 1 ){
			xQueueSend(xQueueG, &valor, 0);
			//delay_ms(150);
		}
		if (but3 == 1){
			xQueueSend(xQueueB, &valor, 0);
			//delay_ms(150);
		}
	}
}

static void task_led_r (void *pvParameters){
	int fila = 0; 
	while(1){
		if( xQueueReceive( xQueueR, &fila, ( TickType_t ) 0 )){
						printf("Afec r %d \n",fila);

		}
		
			pio_set(LED_R_PIO, LED_R_IDX_MASK);
			delay_ms(fila);
			pio_clear(LED_R_PIO, LED_R_IDX_MASK);
			delay_ms(20-fila);
		
		
	}
}
static void task_led_g (void *pvParameters){
	int fila2 = 0;
	while(1){
		if( xQueueReceive(xQueueG, &fila2, ( TickType_t ) 0 )){
						printf("Afec g %d \n",fila2);

		}
	
		pio_set(LED_G_PIO, LED_G_IDX_MASK);
		delay_ms(fila2);
		pio_clear(LED_G_PIO, LED_G_IDX_MASK);
		delay_ms(20-fila2);
		
		
	}
}

static void task_led_b (void *pvParameters){
	int fila3 = 0;
	while(1){
		if( xQueueReceive( xQueueB, &fila3, ( TickType_t ) 0 )){
						printf("Afec b %d \n",fila3);

		}
		
		pio_set(LED_B_PIO, LED_B_IDX_MASK);
		delay_ms(fila3);
		pio_clear(LED_B_PIO, LED_B_IDX_MASK);
		delay_ms(20-fila3);
		
		
	}
}
/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

static void configure_console(void) {
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.charlength = CONF_UART_CHAR_LENGTH,
		.paritytype = CONF_UART_PARITY,
		.stopbits = CONF_UART_STOP_BITS,
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}
static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel,
afec_callback_t callback) {
	/*************************************
	* Ativa e configura AFEC
	*************************************/
	/* Ativa AFEC - 0 */
	afec_enable(afec);

	/* struct de configuracao do AFEC */
	struct afec_config afec_cfg;

	/* Carrega parametros padrao */
	afec_get_config_defaults(&afec_cfg);

	/* Configura AFEC */
	afec_init(afec, &afec_cfg);

	/* Configura trigger por software */
	afec_set_trigger(afec, AFEC_TRIG_SW);

	/*** Configuracao espec?fica do canal AFEC ***/
	struct afec_ch_config afec_ch_cfg;
	afec_ch_get_config_defaults(&afec_ch_cfg);
	afec_ch_cfg.gain = AFEC_GAINVALUE_0;
	afec_ch_set_config(afec, afec_channel, &afec_ch_cfg);

	/*
	* Calibracao:
	* Because the internal ADC offset is 0x200, it should cancel it and shift
	down to 0.
	*/
	afec_channel_set_analog_offset(afec, afec_channel, 0x200);

	/***  Configura sensor de temperatura ***/
	struct afec_temp_sensor_config afec_temp_sensor_cfg;

	afec_temp_sensor_get_config_defaults(&afec_temp_sensor_cfg);
	afec_temp_sensor_set_config(afec, &afec_temp_sensor_cfg);

	/* configura IRQ */
	afec_set_callback(afec, afec_channel, callback, 1);
	NVIC_SetPriority(afec_id, 4);
	NVIC_EnableIRQ(afec_id);
}
void io_init(void) {
	pmc_enable_periph_clk(LED_1_PIO_ID);
	pmc_enable_periph_clk(LED_2_PIO_ID);
	pmc_enable_periph_clk(LED_3_PIO_ID);
	pmc_enable_periph_clk(BUT_1_PIO_ID);
	pmc_enable_periph_clk(BUT_2_PIO_ID);
	pmc_enable_periph_clk(BUT_3_PIO_ID);

	pio_configure(LED_1_PIO, PIO_OUTPUT_0, LED_1_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED_2_PIO, PIO_OUTPUT_0, LED_2_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED_3_PIO, PIO_OUTPUT_0, LED_3_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED_R_PIO, PIO_OUTPUT_0, LED_R_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED_G_PIO, PIO_OUTPUT_0, LED_G_IDX_MASK, PIO_DEFAULT);
	pio_configure(LED_B_PIO, PIO_OUTPUT_0, LED_B_IDX_MASK, PIO_DEFAULT);
	
	pio_configure(BUT_1_PIO, PIO_INPUT, BUT_1_IDX_MASK,
	PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT_2_PIO, PIO_INPUT, BUT_2_IDX_MASK,
	PIO_PULLUP | PIO_DEBOUNCE);
	pio_configure(BUT_3_PIO, PIO_INPUT, BUT_3_IDX_MASK,
	PIO_PULLUP | PIO_DEBOUNCE);

	pio_handler_set(BUT_1_PIO, BUT_1_PIO_ID, BUT_1_IDX_MASK, PIO_IT_EDGE,
	but1_callback);
	pio_handler_set(BUT_2_PIO, BUT_2_PIO_ID, BUT_2_IDX_MASK, PIO_IT_EDGE,
	but2_callback);
	pio_handler_set(BUT_3_PIO, BUT_3_PIO_ID, BUT_3_IDX_MASK, PIO_IT_EDGE,
	but3_callback);

	pio_enable_interrupt(BUT_1_PIO, BUT_1_IDX_MASK);
	pio_enable_interrupt(BUT_2_PIO, BUT_2_IDX_MASK);
	pio_enable_interrupt(BUT_3_PIO, BUT_3_IDX_MASK);

	pio_get_interrupt_status(BUT_1_PIO);
	pio_get_interrupt_status(BUT_2_PIO);
	pio_get_interrupt_status(BUT_3_PIO);

	NVIC_EnableIRQ(BUT_1_PIO_ID);
	NVIC_SetPriority(BUT_1_PIO_ID, 4);

	NVIC_EnableIRQ(BUT_2_PIO_ID);
	NVIC_SetPriority(BUT_2_PIO_ID, 4);

	NVIC_EnableIRQ(BUT_3_PIO_ID);
	NVIC_SetPriority(BUT_3_PIO_ID, 4);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/

int main(void) {
	/* Initialize the SAM system */
	sysclk_init();
	board_init();
	configure_console();
	io_init();

	xQueueBtn = xQueueCreate(32, sizeof(struBut) );
	if (xQueueBtn == NULL){
		printf("falha em criar a fila \n");
	}
	
	xQueueAFEC = xQueueCreate(100, sizeof(uint) );
	if (xQueueAFEC == NULL){
		printf("falha em criar a fila \n");
	}
	xQueueR = xQueueCreate(1, sizeof(int));
	if (xQueueR == NULL){
		printf("falha em criar a fila \n");
	}
	xQueueG = xQueueCreate(1, sizeof(int));
	if (xQueueG == NULL){
		printf("falha em criar a fila \n");
	}
	xQueueB = xQueueCreate(1, sizeof(int));
	if (xQueueB == NULL){
		printf("falha em criar a fila \n");
	}
	if (xTaskCreate(task_oled, "oled", TASK_OLED_STACK_SIZE, NULL,
	TASK_OLED_STACK_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create oled task\r\n");
	}
	if (xTaskCreate(task_color, "color", 1024, NULL, 0, NULL) != pdPASS) {
		printf("Failed to create test color task\r\n");
	}
	if (xTaskCreate(task_led_r, "color", 1024, NULL, 0, NULL) != pdPASS) {
		printf("Failed to create led_r task\r\n");
	}
	if (xTaskCreate(task_led_g, "color", 1024, NULL, 0, NULL) != pdPASS) {
		printf("Failed to create led_g task\r\n");
	}
	if (xTaskCreate(task_led_b, "color", 1024, NULL, 0, NULL) != pdPASS) {
		printf("Failed to create led_b task\r\n");
	}
	vTaskStartScheduler();
	while (1) {
	}

	return 0;
}
