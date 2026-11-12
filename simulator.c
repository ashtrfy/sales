#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define MAX_PRODUCTS 100
#define MAX_CUSTOMERS 100
#define SALES_FILE "sales.csv"
#define LOG_FILE "log.txt"

typedef struct {
int code;
char name[64];
float price;
} Product;

typedef struct {
int code;
char name[128];
} Customer;

Product products[MAX_PRODUCTS];
int products_count = 0;

Customer customers_list[MAX_CUSTOMERS];
int customers_count = 0;

pthread_mutex_t file_mutex;
pthread_mutex_t log_mutex;

int next_sale_id = 1;

void write_log(const char* message) {
pthread_mutex_lock(&log_mutex);
FILE* log = fopen(LOG_FILE, "a");
if (log != NULL) {
time_t now = time(NULL);
char time_buf[64];
struct tm* tm_info = localtime(&now);
strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
fprintf(log, "[%s] %s\n", time_buf, message);
fclose(log);
}
pthread_mutex_unlock(&log_mutex);
}

void load_products() {
FILE* f = fopen("products.csv", "r");
if (f == NULL) {
write_log("ОШИБКА: products.csv не найден.");
return;
}
char header[256];
fgets(header, sizeof(header), f);
while (fscanf(f, "%d,%63[^,],%f",
&products[products_count].code,
products[products_count].name,
&products[products_count].price) == 3) {
products_count++;
}
fclose(f);
}

void load_customers() {
FILE* f = fopen("customers.csv", "r");
if (f == NULL) {
write_log("ОШИБКА: customers.csv не найден.");
return;
}
char header[256];
fgets(header, sizeof(header), f);
char fam[32], imya[32], otch[32];

while (fscanf(f, "%d,%31[^,],%31[^,],%31[^\n]", 
&customers_list[customers_count].code, 
fam, imya, otch) == 4) {
snprintf(customers_list[customers_count].name, 
sizeof(customers_list[customers_count].name), 
"%s %s %s", fam, imya, otch);
customers_count++;
}
fclose(f);
}

void* sell_product(void* arg) {
int product_index = *(int*)arg;
Product p = products[product_index];
int interval = 1 + rand() % 3;
int sales_to_make = 1 + rand() % 15;

for (int i = 0; i < sales_to_make; i++) {
sleep(interval);

pthread_mutex_lock(&file_mutex);

FILE* f = fopen(SALES_FILE, "a");
if (f != NULL) {
int sale_id = next_sale_id++;
int random_customer = rand() % customers_count;
int customer_code = customers_list[random_customer].code;

time_t now = time(NULL);
char time_buf[64];
struct tm* tm_info = localtime(&now);
strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H:%M:%S", tm_info);

fprintf(f, "%d\t%s\t%d\t%d\n",
sale_id, time_buf, customer_code, p.code);
fclose(f);

printf("Поток %d записал продажу ID=%d (Купил: %s)\n", 
product_index + 1, sale_id, customers_list[random_customer].name);
fflush(stdout);

char msg[256];
snprintf(msg, sizeof(msg),
"Продажа: ID=%d, товар=%s, покупатель=%s",
sale_id, p.name, customers_list[random_customer].name);
write_log(msg);
}

pthread_mutex_unlock(&file_mutex);
}
return NULL;
}

int main() {
srand(time(NULL));

pthread_mutex_init(&file_mutex, NULL);
pthread_mutex_init(&log_mutex, NULL);

load_products();
load_customers();

printf("Загружено товаров: %d, покупателей: %d\n", products_count, customers_count);

if (products_count == 0) {
printf("Нет товаров для имитации продаж. Проверьте products.csv\n");
return 1;
}
if (customers_count == 0) {
printf("Нет покупателей. Проверьте customers.csv\n");
return 1;
}

FILE* f = fopen(SALES_FILE, "w");
if (f != NULL) {
fprintf(f, "sale_id\tdate_time\tcustomer_code\tproduct_code\n");
fclose(f);
}

pthread_t threads[MAX_PRODUCTS];
int thread_args[MAX_PRODUCTS];

for (int i = 0; i < products_count; i++) {
thread_args[i] = i;
pthread_create(&threads[i], NULL, sell_product, &thread_args[i]);
}

for (int i = 0; i < products_count; i++) {
pthread_join(threads[i], NULL);
}

printf("Симуляция завершена\n");

pthread_mutex_destroy(&file_mutex);
pthread_mutex_destroy(&log_mutex);
return 0;
}
