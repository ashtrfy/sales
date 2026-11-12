#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_PRODUCTS 100
#define MAX_CUSTOMERS 100
#define MAX_SALES 1000
#define REPORT_FILE "report.txt"
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

typedef struct {
int sale_id;
char date_time[64];
int customer_code;
int product_code;
} Sale;

Product products[MAX_PRODUCTS];
int products_count = 0;

Customer customers_list[MAX_CUSTOMERS];
int customers_count = 0;

Sale sales[MAX_SALES];
int sales_count = 0;

float threshold_sum = 5000.0;
pthread_mutex_t log_mutex;
pthread_mutex_t param_mutex;

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

Product* find_product(int code) {
for (int i = 0; i < products_count; i++) {
if (products[i].code == code) return &products[i];
}
return NULL;
}

Customer* find_customer(int code) {
for (int i = 0; i < customers_count; i++) {
if (customers_list[i].code == code) return &customers_list[i];
}
return NULL;
}

void load_static_data() {
FILE* f = fopen("products.csv", "r");
if (f != NULL) {
char header[256];
fgets(header, sizeof(header), f);
while (fscanf(f, "%d,%63[^,],%f", &products[products_count].code, products[products_count].name, &products[products_count].price) == 3) {
products_count++;
}
fclose(f);
}

f = fopen("customers.csv", "r");
if (f != NULL) {
char header[256];
fgets(header, sizeof(header), f);
char fam[32], imya[32], otch[32];
while (fscanf(f, "%d,%31[^,],%31[^,],%31[^\n]", &customers_list[customers_count].code, fam, imya, otch) == 4) {
snprintf(customers_list[customers_count].name, sizeof(customers_list[customers_count].name), "%s %s %s", fam, imya, otch);
customers_count++;
}
fclose(f);
}
}

void load_sales_data() {
sales_count = 0;
FILE* f = fopen("sales.csv", "r");
if (f != NULL) {
char header[256];
fgets(header, sizeof(header), f);
while (fscanf(f, "%d\t%63s\t%d\t%d", &sales[sales_count].sale_id, sales[sales_count].date_time, &sales[sales_count].customer_code, &sales[sales_count].product_code) == 4) {
sales_count++;
if (sales_count >= MAX_SALES) break;
}
fclose(f);
}
}

void* thread_total_sum(void* arg) {
float total = 0;
for (int i = 0; i < sales_count; i++) {
Product* p = find_product(sales[i].product_code);
if (p) total += p->price;
}
printf("[Поток 1] Общая сумма продаж: %.2f руб.\n", total);
fflush(stdout);
return NULL;
}

typedef struct {
int code;
int count;
} ProductStat;

void* thread_popular(void* arg) {
ProductStat stats[MAX_PRODUCTS];
int stat_count = 0;
for (int i = 0; i < products_count; i++) {
stats[i].code = products[i].code;
stats[i].count = 0;
}
stat_count = products_count;

for (int i = 0; i < sales_count; i++) {
for (int j = 0; j < stat_count; j++) {
if (stats[j].code == sales[i].product_code) {
stats[j].count++;
break;
}
}
}

for (int i = 0; i < stat_count - 1; i++) {
for (int j = i + 1; j < stat_count; j++) {
if (stats[i].count < stats[j].count) {
ProductStat temp = stats[i];
stats[i] = stats[j];
stats[j] = temp;
}
}
}

printf("[Поток 2] ТОП-5 популярных товаров:\n");
for (int i = 0; i < 5 && i < stat_count; i++) {
Product* p = find_product(stats[i].code);
printf(" %d. %s (продано: %d)\n", i + 1, p ? p->name : "?", stats[i].count);
}

printf("[Поток 2] ТОП-5 непопулярных товаров:\n");
int limit = stat_count > 5 ? 5 : stat_count;
for (int i = 1; i <= limit; i++) {
int index = stat_count - i;
Product* p = find_product(stats[index].code);
printf(" %d. %s (продано: %d)\n", i, p ? p->name : "?", stats[index].count);
}
fflush(stdout);
return NULL;
}

void* thread_customers(void* arg) {
float* sums = calloc(customers_count, sizeof(float));
for (int i = 0; i < sales_count; i++) {
Product* p = find_product(sales[i].product_code);
if (p) {
for (int j = 0; j < customers_count; j++) {
if (customers_list[j].code == sales[i].customer_code) {
sums[j] += p->price;
break;
}
}
}
}

pthread_mutex_lock(&param_mutex);
float current_threshold = threshold_sum;
pthread_mutex_unlock(&param_mutex);

printf("[Поток 3] Покупатели с суммой покупок > %.2f руб.:\n", current_threshold);
int found = 0;
for (int i = 0; i < customers_count; i++) {
if (sums[i] > current_threshold) {
printf(" - %s (Сумма: %.2f руб.)\n", customers_list[i].name, sums[i]);
found = 1;
}
}
if (!found) printf(" (Нет таких покупателей)\n");
fflush(stdout);
free(sums);
return NULL;
}

void* thread_trends(void* arg) {
printf("[Поток 4] Анализ тенденций: ");
if (sales_count > 30) {
printf("Объем продаж высокий, спрос стабилен.\n");
} else if (sales_count > 10) {
printf("Объем продаж умеренный.\n");
} else if (sales_count > 0) {
printf("Низкий уровень продаж.\n");
} else {
printf("Продаж нет.\n");
}
fflush(stdout);
return NULL;
}

void generate_report() {
FILE* out = fopen(REPORT_FILE, "w");
if (!out) return;

fprintf(out, "ИТОГОВЫЙ ОТЧЕТ ПО ПРОДАЖАМ\n");
fprintf(out, "Всего записей о продажах: %d\n\n", sales_count);

float total = 0;
for (int i = 0; i < sales_count; i++) {
Product* p = find_product(sales[i].product_code);
if (p) total += p->price;
}
fprintf(out, "Общая выручка: %.2f руб.\n\n", total);

fprintf(out, "ДЕТАЛИЗАЦИЯ ПРОДАЖ:\n");
fprintf(out, "ID | Дата/Время | Товар | Цена | Покупатель\n");
for (int i = 0; i < sales_count; i++) {
Product* p = find_product(sales[i].product_code);
Customer* c = find_customer(sales[i].customer_code);
fprintf(out, "%-4d | %-19s | %-15s | %8.2f | %s\n",
sales[i].sale_id, sales[i].date_time, p ? p->name : "?", p ? p->price : 0.0, c ? c->name : "?");
}
fclose(out);
write_log("Отчет успешно сгенерирован.");
}

int main() {
pthread_mutex_init(&log_mutex, NULL);
pthread_mutex_init(&param_mutex, NULL);
write_log("Старт программы-анализатора.");

load_static_data();
printf("Справочники загружены: товаров %d, покупателей %d\n", products_count, customers_count);

int choice;
do {
printf("\nМЕНЮ АНАЛИЗАТОРА:\n");
printf("1. Проанализировать текущие продажи\n");
printf("2. Изменить порог суммы покупателя (сейчас: %.2f)\n", threshold_sum);
printf("3. Сгенерировать отчет report.txt\n");
printf("4. Выход\n");
printf("Выбор: ");
scanf("%d", &choice);

if (choice == 1) {
load_sales_data();
printf("\n[Анализатор] Считано актуальных записей из sales.csv: %d\n", sales_count);

if (sales_count == 0) {
printf("Пока нет продаж. Подождите, пока первая программа их запишет.\n");
continue;
}

pthread_t t1, t2, t3, t4;
printf("Запуск потоков анализа\n");
pthread_create(&t1, NULL, thread_total_sum, NULL);
pthread_create(&t2, NULL, thread_popular, NULL);
pthread_create(&t3, NULL, thread_customers, NULL);
pthread_create(&t4, NULL, thread_trends, NULL);

pthread_join(t1, NULL);
pthread_join(t2, NULL);
pthread_join(t3, NULL);
pthread_join(t4, NULL);
printf("Анализ завершен\n");
} else if (choice == 2) {
pthread_mutex_lock(&param_mutex);
printf("Введите новый порог: ");
scanf("%f", &threshold_sum);
pthread_mutex_unlock(&param_mutex);
printf("Порог изменен на %.2f\n", threshold_sum);
write_log("Изменен порог суммы покупателя.");
} else if (choice == 3) {
load_sales_data();
generate_report();
printf("Отчет сохранен в report.txt\n");
}
} while (choice != 4);

pthread_mutex_destroy(&log_mutex);
pthread_mutex_destroy(&param_mutex);
return 0;
}

