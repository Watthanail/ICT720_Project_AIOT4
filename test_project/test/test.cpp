#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

// Queue handle
QueueHandle_t data_queue;

// Task function prototypes
void producer_task(void *pvParameters);
void consumer_task(void *pvParameters);

// Semaphore handle to synchronize tasks
SemaphoreHandle_t semaphore;

void setup() {
  Serial.begin(115200);

  // Create a queue to hold float values
  data_queue = xQueueCreate(5, sizeof(float));

  // Create the semaphore
  semaphore = xSemaphoreCreateBinary();

  // Create the producer task
  xTaskCreate(producer_task, "ProducerTask", 1000, NULL, 1, NULL);

  // Create the consumer task
  xTaskCreate(consumer_task, "ConsumerTask", 1000, NULL, 1, NULL);
}

void loop() {
  // Empty, as Arduino loop is not used in conjunction with FreeRTOS
}

void producer_task(void *pvParameters) {
  float data = 0.0;
  TickType_t last_wake_time = xTaskGetTickCount();

  while (true) {
    // Produce data
    data += 0.1;

    // Send data to the queue
    xQueueSend(data_queue, &data, portMAX_DELAY);

    Serial.print("Producer: Sent data ");
    Serial.println(data);

    // Check if data > 1 and release semaphore
    if (data > 1) {
      xSemaphoreGive(semaphore);
    }

    // Delay before producing the next data
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
  }
}

void consumer_task(void *pvParameters) {
  float received_data;

  while (true) {
    // Wait for semaphore to be released
    if (xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE) {
      // Receive data from the queue
      xQueueReceive(data_queue, &received_data, portMAX_DELAY);

      Serial.print("Consumer: Received data ");
      Serial.println(received_data);

      // Process the received data (for example, print it)
      // Note: In a real-world scenario, you would do more meaningful processing here

      // Simulate processing time
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}
