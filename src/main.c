/*
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
#define SEG_G 2

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
    gpio_set_level(SEG_A, !digitos[numero][0]);
    gpio_set_level(SEG_B, !digitos[numero][1]);
    gpio_set_level(SEG_C, !digitos[numero][2]);
    gpio_set_level(SEG_D, !digitos[numero][3]);
    gpio_set_level(SEG_E, !digitos[numero][4]);
    gpio_set_level(SEG_F, !digitos[numero][5]);
    gpio_set_level(SEG_G, !digitos[numero][6]);
}

// ==================== ISR DEL TIMER ====================
// Esta rutina se ejecuta cada 20 ms.
// Su función es revisar los botones sin usar delay.
static bool IRAM_ATTR timer_isr(void *arg) {
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

    return false;
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
} */

/*
////////////////PUNTO 2!!!!!!////////////////////////////////////////

// PUNTO 2 - SEMÁFORO PEATONAL
//-mismas librerías seimpre así no las use
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"

//================defino siempre los PINES =============
#define LED_VERDE     25  //de un lado
#define LED_AMARILLO  26
#define LED_ROJO      27

#define BTN_S1        32  //del otro lado

// ==========defino TIMER ====================
// 80 MHz / 80 = 1 MHz, o sea: 1 tick = 1 microsegundo
//Piensas directamente en microsegundos.

#define TIMER_DIVIDER 80
#define TIMER_INTERVAL_US 20000   // 20 ms

// ==================== TIEMPOS ====================
// Los pongo en milisegundos
#define TIEMPO_VERDE_MS    5000
#define TIEMPO_AMARILLO_MS 2000
#define TIEMPO_ROJO_MS     5000
#define TIEMPO_EXTRA_MS    1000

// ==================== ESTADOS ====================

typedef enum { //Agrupa opciones fijas
    VERDE,
    AMARILLO,
    ROJO
} estado_t; //tipo de dato para guardar en el estado del semáforo

static volatile estado_t estado = VERDE; //variable principal del semáforo, inicia en verde

static volatile int tiempo_estado = 0; //guarda cuanto tiempo en el estado actual, para cambiar de color

static volatile bool actualizar_leds = true; //es una bandera

static volatile bool estado_anterior_boton = 1; //para detectar cuando presiona
static volatile bool solicitud_peaton = false;  //guarda si ya hubo solicitud o no
static volatile int tiempo_boton = 0; //guarda el tiempo en que se presionó


// ==================== CONTROL DE LEDS ====================

void actualizar_semaforo(estado_t e) {

    gpio_set_level(LED_VERDE, 0);
    gpio_set_level(LED_AMARILLO, 0);
    gpio_set_level(LED_ROJO, 0);

    if (e == VERDE) { //prenda cada led
        gpio_set_level(LED_VERDE, 1);
    }

    if (e == AMARILLO) {
        gpio_set_level(LED_AMARILLO, 1);
    }

    if (e == ROJO) {
        gpio_set_level(LED_ROJO, 1);
    }
}


// ==================== ISR TIMER ====================

static bool IRAM_ATTR timer_isr(void *arg) { //siemmpre lo defino así

    bool estado_actual = gpio_get_level(BTN_S1); //va a captar en que estado el botón

    if ((estado_anterior_boton == 1) && (estado_actual == 0)) { //detecta flanco de bajada

        if (estado == VERDE) { //si se presiona solo me importa si está en verde, de resto no porque los peatones pueden pasar
            solicitud_peaton = true; //se activo la solicitud por el botón
            tiempo_boton = tiempo_estado; //permite calcular cuanto tiempo ha pasado desde la solicitud
        }
    }

    estado_anterior_boton = estado_actual; //para actualizar el estado del botón

    tiempo_estado += 20; //aumentar el tiempo del estado actual, cronómetro interno del estado

    if (estado == VERDE) {

        if (tiempo_estado >= TIEMPO_VERDE_MS) { //cuando supera tiempo de estado, pasa a amarillo 

            estado = AMARILLO;
            tiempo_estado = 0;
            solicitud_peaton = false;
            actualizar_leds = true; //tiene que actualizar
        }

        else if (solicitud_peaton &&
                (tiempo_estado - tiempo_boton >= TIEMPO_EXTRA_MS)) {//hubo solicitud, cuanto tiempo ha pasado desde que pidió cambio
            estado = AMARILLO;
            tiempo_estado = 0; //se reinicia porque acabo de cambiar
            solicitud_peaton = false; //borra solicitud porque ya cumplió
            actualizar_leds = true;
        }
    }

    else if (estado == AMARILLO) {

        if (tiempo_estado >= TIEMPO_AMARILLO_MS) { //cuanto tiempo esta y si superó el tiempo en que tiene para estar ahí

            estado = ROJO; //cambia al rojo que sigue
            tiempo_estado = 0; //reinicia para empezar a contar ahí
            actualizar_leds = true;
        }
    }

    else if (estado == ROJO) { //misma lógica

        if (tiempo_estado >= TIEMPO_ROJO_MS) {

            estado = VERDE; //ya cambia a verde
            tiempo_estado = 0;
            actualizar_leds = true;
        }
    }

    return false; //siempreee
}


// ==================== MAIN ====================

void app_main() {

    gpio_config_t out_cfg = { //configuración del out

        .pin_bit_mask =
            (1ULL << LED_VERDE) | //siempre declaro así
            (1ULL << LED_AMARILLO) |
            (1ULL << LED_ROJO),

        .mode = GPIO_MODE_OUTPUT, //modo out
        .pull_up_en = GPIO_PULLUP_DISABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE //siempreee estos tres disable
    }; 

    gpio_config(&out_cfg); //siempre esto


    gpio_config_t in_cfg = { //modo in

        .pin_bit_mask = (1ULL << BTN_S1), //es lo único que entra, si tuviera más botones, aquí

        .mode = GPIO_MODE_INPUT, //modo in
        .pull_up_en = GPIO_PULLUP_ENABLE, //este enable los otros disable
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&in_cfg); //siempre pongo esto al final


    actualizar_semaforo(estado); //siempre mantengo actualizando, de eso depende


    timer_config_t timer_cfg = { //siempre denomino así el timer

        .divider = TIMER_DIVIDER, // lo defini antes = 80
        .counter_dir = TIMER_COUNT_UP, //Va a contar hacia arriba
        .counter_en = TIMER_PAUSE, //se configura inicialmente en pausa
        .alarm_en = TIMER_ALARM_EN,//se activa la alarma del timer
        .auto_reload = true //se reinicie automáticamente
    };

    timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg); //siempre

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0); //lo pongo en cero

    timer_set_alarm_value( //defino cuando se activa la alarma
        TIMER_GROUP_0,
        TIMER_0,
        TIMER_INTERVAL_US //lo definí arriba = 20ms
    );

    timer_isr_callback_add( //siempre va
        TIMER_GROUP_0,
        TIMER_0,
        timer_isr,
        NULL,
        0
    );

    timer_enable_intr(TIMER_GROUP_0, TIMER_0); //activo la interrupción

    timer_start(TIMER_GROUP_0, TIMER_0); //cada 20 ms se activa ISR


    while (1) { //siempre aquí lo minimo

        if (actualizar_leds) {

            actualizar_semaforo(estado);

            actualizar_leds = false;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
} */