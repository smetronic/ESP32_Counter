/*
Author: Smetronic
Date: 2020-11-26
Sate: Tested = Working, WatchDog
*/

extern "C" {
#include "soc/pcnt_struct.h"
}
#include "driver/pcnt.h"
#include <EEPROM.h>
#include <arduino-timer.h>
auto timer = timer_create_default();

const int ledPin = 2;

byte pulsePin = 34; // Pulse Counter

int16_t flowCounter = 0;
int16_t Pulses = 0;
int16_t x;
volatile uint32_t us_time, us_time_diff;


volatile byte state = LOW;
volatile byte state2 = LOW;
volatile byte state_tmr = 0;
volatile byte value_ready = 0;

#define PCNT_TEST_UNIT PCNT_UNIT_0
#define PCNT_H_LIM_VAL 32767
#define PCNT_L_LIM_VAL -1

typedef struct {
	volatile uint64_t total_accumulator;
	volatile int16_t counter;
} variable_t __attribute__((packed));

variable_t variable{ 0, 0 };

bool debug = false;
bool watchdog = false;

void setup() {

	pinMode(pulsePin, INPUT);
	Serial.begin(9600);
	Serial2.begin(9600);

	InitCounter();

	LoadStruct(&variable, sizeof(variable));

	pinMode(ledPin, OUTPUT);
	digitalWrite(ledPin, LOW);

	pinMode(18, INPUT);

	timer.every(1000, count);
}

int seconds = 1;


bool count(void*) {

	if (watchdog) {
		seconds++;
	}
	return true;
}

volatile uint64_t prior_count = 0;
long previousMillis = 0;

void loop() {
	CalculatePulses();

	timer.tick();

	Serial.printf("[%llu]", (variable.total_accumulator + variable.counter));
	Serial.println(seconds);

	if (seconds >= 60 && watchdog) {
		digitalWrite(ledPin, HIGH);

		pinMode(18, OUTPUT);
		digitalWrite(18, LOW);

		delay(1000);
		ESP.restart();
	}

	


	Serial2.printf("[%llu]", (variable.total_accumulator + variable.counter));
	Serial2.println();



	if (Serial2.available() > 0) {
		seconds = 0;

		int feedback = Serial2.read();

		if (feedback == 2)
		{
			watchdog = true;
			Serial.println("Watchdog Enabled");
		}

		if (feedback == 9) {

			variable.total_accumulator = 0;
			variable.counter = 0;

			pcnt_counter_clear(PCNT_TEST_UNIT);
			pcnt_counter_resume(PCNT_TEST_UNIT);

			delay(300);

			StoreStruct(&variable, sizeof(variable));
			ESP.restart();
		}

		delay(300);

		Serial2.flush();
	}

	delay(500);

	unsigned long currentMillis = millis();

	//Save the Values in Memory
	if ((currentMillis - previousMillis >= 5000) && variable.total_accumulator != prior_count) {
		prior_count = variable.total_accumulator;
		previousMillis = currentMillis;

		StoreStruct(&variable, sizeof(variable));

		delay(100);

		if (debug) {
			Serial.println("Saved");
		}
	}

}

void InitCounter() {

	pcnt_config_t pcnt_config = {
	pulsePin, // Pulse input gpio_num, if you want to use gpio16, pulse_gpio_num = 16, a negative value will be ignored
	PCNT_PIN_NOT_USED, // Control signal input gpio_num, a negative value will be ignored
	PCNT_MODE_KEEP, // PCNT low control mode
	PCNT_MODE_KEEP, // PCNT high control mode
	PCNT_COUNT_INC, // PCNT positive edge count mode
	PCNT_COUNT_DIS, // PCNT negative edge count mode
	PCNT_H_LIM_VAL, // Maximum counter value
	PCNT_L_LIM_VAL, // Minimum counter value
	PCNT_TEST_UNIT, // PCNT unit number
	PCNT_CHANNEL_0, // the PCNT channel
	};

	if (pcnt_unit_config(&pcnt_config) == ESP_OK) //init unit
		Serial.println("Config Unit_0 = ESP_OK");

	pcnt_filter_enable(PCNT_TEST_UNIT);

	pcnt_intr_disable(PCNT_TEST_UNIT);
	pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_L_LIM);
	pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_H_LIM);
	pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_THRES_0);
	pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_THRES_1);
	pcnt_event_disable(PCNT_TEST_UNIT, PCNT_EVT_ZERO);

	pcnt_counter_pause(PCNT_TEST_UNIT);

	pcnt_counter_clear(PCNT_TEST_UNIT);

	pcnt_intr_enable(PCNT_TEST_UNIT);

	pcnt_counter_resume(PCNT_TEST_UNIT);

}

void CalculatePulses() {
	if (pcnt_get_counter_value(PCNT_TEST_UNIT, &flowCounter) == ESP_OK)
	{
		variable.counter = flowCounter;

		if (flowCounter >= 50) {
			variable.total_accumulator += flowCounter;

			pcnt_counter_clear(PCNT_TEST_UNIT);
			pcnt_counter_resume(PCNT_TEST_UNIT);
		}
	}

	if (debug) {
		Serial.print("FlowCounter: ");
		Serial.println(flowCounter);

		Serial.printf("Counter: %" PRId16 "\n", variable.counter);

		Serial.printf("Accumulator: %llu, Total: %llu", variable.total_accumulator, (variable.total_accumulator + variable.counter));
		Serial.println();

		delay(1000);
	}
}

void StoreStruct(void* data_source, size_t size)
{
	EEPROM.begin(size * 2);
	for (size_t i = 0; i < size; i++)
	{
		char data = ((char*)data_source)[i];
		EEPROM.write(i, data);
	}
	EEPROM.commit();
}

void LoadStruct(void* data_dest, size_t size)
{
	EEPROM.begin(size * 2);
	for (size_t i = 0; i < size; i++)
	{
		char data = EEPROM.read(i);
		((char*)data_dest)[i] = data;
	}
}
