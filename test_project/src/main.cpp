#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

// Task function prototypes
void frequentPrintTask(void *pvParameters);
void lessFrequentPrintTask(void *pvParameters);

void setup() {
  Serial.begin(9600);

  // Introduce a delay before creating tasks to start them at the same time
  delay(1000);

  // Create the frequent print task (higher priority)
  xTaskCreate(frequentPrintTask, "FrequentPrintTask", 1000, NULL, 2, NULL);

  // Create the less frequent print task (lower priority)
  xTaskCreate(lessFrequentPrintTask, "LessFrequentPrintTask", 1000, NULL, 1, NULL);
}

void loop() {
  // Empty, as Arduino loop is not used in conjunction with FreeRTOS
}

void frequentPrintTask(void *pvParameters) {
  while (true) {
    // Print message from the frequent print task
    Serial.print(millis());
    Serial.println(": task A executing...");

    // Adjust the delay value for frequent printing
    vTaskDelay(pdMS_TO_TICKS(200)); // Print every 200ms
  }
}

void lessFrequentPrintTask(void *pvParameters) {
  while (true) {
    // Print message from the less frequent print task
    Serial.print(millis());
    Serial.println(": task B executing...");

    // Adjust the delay value for less frequent printing
    vTaskDelay(pdMS_TO_TICKS(1000)); // Print every 1 second
  }
}


//if task B has more priority than task A
//7432: task B executing...
//7432: task A executing...
//task B will preempt task A to print by printinnng it first

//if task A has prior 2 and B has prior 1  (priority A more than B)
//9140: task A executing...
//9140: task B executing...