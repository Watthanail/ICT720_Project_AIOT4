#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

// Queue handle
QueueHandle_t data_queue;

// Task function prototypes
void producer_task(void *pvParameters);
void consumer_task(void *pvParameters);

void setup() {
  Serial.begin(115200);

  // Create a queue to hold float values
  data_queue = xQueueCreate(5, sizeof(float));

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

    // Delay before producing the next data
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(2000));
  }
}

void consumer_task(void *pvParameters) {
  float received_data;

  while (true) {
    UBaseType_t items_waiting = uxQueueMessagesWaiting(data_queue);

    // Print the number of waiting data in the queue
    Serial.print("Consumer: Number of waiting data in queue: ");
    Serial.println(items_waiting);
    // Receive data from the queue
    xQueueReceive(data_queue, &received_data, portMAX_DELAY);

    Serial.print("Consumer: Received data ");
    Serial.println(received_data);

    vTaskDelay(pdMS_TO_TICKS(2200));
    // this will be shown as the time tick for sending more than recieve so after the item reached the limit of queue.
    //Blocking behavior (portMAX_DELAY used): If the queue is full and the producer task attempts to send data using xQueueSend() with portMAX_DELAY as the block time, the producer task will block (i.e., wait) until space becomes available in the queue. This means the producer task will pause execution until there is room in the queue to send the data. This could potentially delay other tasks in the system if they are dependent on the producer task.

    //Non-blocking behavior (zero block time): If the queue is full and the producer task attempts to send data using xQueueSend() with a block time of zero, the function will return immediately with an error code (usually errQUEUE_FULL). This allows the producer task to handle the situation accordingly, such as retrying after some time or implementing a different strategy.
  }
}
