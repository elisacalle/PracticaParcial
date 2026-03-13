// PUNTO 1 - CONTADOR 0 A 9 EN DISPLAY DE 7 SEGMENTOS
// botones para ascendente y descendente

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"

// Defino los pines
// Segmentos del display
#define SEG_A 4
#define SEG_B 16
#define SEG_C 17
#define SEG_D 18
#define SEG_E 19
#define SEG_F 21
#define SEG_G 22

// Defino los pulsadores
#define BTN_S1 32   // Para incrementar
#define BTN_S2 33   // Para decrementar

// Timer
#define TIMER_DIVIDER 80              // 80 MHz / 80 = 1 MHz -> 1 tick = 1 us
#define TIMER_BASE_CLK 1000000        // 1 MHz
#define TIMER_INTERVAL_US 20000       // 20 ms


// Contador del sistema
static volatile int contador = 0;

// Bandera para indicar que el display debe actualizarse
static volatile bool actualizar_display = true;

// Variables para detectar flanco de pulsación
static volatile bool estado_anterior_s1 = 1;
static volatile bool estado_anterior_s2 = 1;

// ==================== TABLA DE NÚMEROS ====================
// Esta tabla indica qué segmentos deben encenderse para mostrar 0-9
// Orden: A, B, C, D, E, F, G
// 1 = segmento encendido
// 0 = segmento apagado
static const uint8_t digitos[10][7] = {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}  // 9
};

// ==================== FUNCIÓN PARA MOSTRAR NÚMERO ====================
void mostrar_numero(int numero) {
    gpio_set_level(SEG_A, digitos[numero][0]);
    gpio_set_level(SEG_B, digitos[numero][1]);
    gpio_set_level(SEG_C, digitos[numero][2]);
    gpio_set_level(SEG_D, digitos[numero][3]);
    gpio_set_level(SEG_E, digitos[numero][4]);
    gpio_set_level(SEG_F, digitos[numero][5]);
    gpio_set_level(SEG_G, digitos[numero][6]);
}

// ==================== ISR DEL TIMER ====================
// Esta rutina se ejecuta cada 20 ms.
// Su función es revisar los botones sin usar delay.
static void IRAM_ATTR timer_isr(void *arg) {
    // Leer estado actual de los botones
    bool estado_actual_s1 = gpio_get_level(BTN_S1);
    bool estado_actual_s2 = gpio_get_level(BTN_S2);

    // Detectar flanco de bajada en S1:
    // antes estaba en 1 y ahora está en 0 -> botón presionado
    if ((estado_anterior_s1 == 1) && (estado_actual_s1 == 0)) {
        contador++;

        // Si supera 9, vuelve a 0
        if (contador > 9) {
            contador = 0;
        }

        actualizar_display = true; //se debe actualizar siempre
    }

    // Detectar flanco de bajada en S2:
    // antes estaba en 1 y ahora está en 0 -> botón presionado
    if ((estado_anterior_s2 == 1) && (estado_actual_s2 == 0)) {
        contador--;

        // Si baja de 0, vuelve a 9
        if (contador < 0) {
            contador = 9;
        }

        actualizar_display = true; //se debe actualizar siempre
    }

    // Guardar el estado actual para la próxima comparación
    estado_anterior_s1 = estado_actual_s1;
    estado_anterior_s2 = estado_actual_s2;
}

// ==================== FUNCIÓN PRINCIPAL ====================
void app_main() {

    // ----------- CONFIGURACIÓN DE LOS PINES DE SALIDA -----------
    gpio_config_t out_cfg = {
        .pin_bit_mask =
            (1ULL << SEG_A) | //la barra es cuando son varias
            (1ULL << SEG_B) |
            (1ULL << SEG_C) |
            (1ULL << SEG_D) |
            (1ULL << SEG_E) |
            (1ULL << SEG_F) |
            (1ULL << SEG_G),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&out_cfg); //este siempre

    // ----------- CONFIGURACIÓN DE LOS PINES DE ENTRADA -----------
    gpio_config_t in_cfg = {
        .pin_bit_mask =
            (1ULL << BTN_S1) |
            (1ULL << BTN_S2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // pull-up interno
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&in_cfg);

    // ----------- MOSTRAR EL VALOR INICIAL -----------
    mostrar_numero(contador);

    // ----------- CONFIGURACIÓN DEL TIMER -----------
    timer_config_t timer_cfg = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true
    };

    timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg);

    // Iniciar contador del timer en 0
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    // El timer disparará la interrupción cada 20 000 us = 20 ms
    timer_set_alarm_value(
        TIMER_GROUP_0,
        TIMER_0,
        TIMER_INTERVAL_US
    );

    // Registrar la ISR
    timer_isr_callback_add(
        TIMER_GROUP_0,
        TIMER_0,
        timer_isr,
        NULL,
        0
    );

    // Habilitar interrupción del timer
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

    // Iniciar el timer
    timer_start(TIMER_GROUP_0, TIMER_0);

    // ----------- BUCLE PRINCIPAL -----------
    while (1) {
        // Solo actualiza display cuando haya cambio en el contador
        if (actualizar_display) {
            mostrar_numero(contador);
            actualizar_display = false;
        }

        // Pequeña espera para no ocupar toda la CPU
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}