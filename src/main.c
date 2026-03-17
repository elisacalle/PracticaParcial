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
} 

*/











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
} 

*/









/*


//==========PUNTO 4==========================
//============PUNTO 4=========================
//===========PUNTO 4=========================

// PUNTO 4 - CRONÓMETRO 00 A 59 SEGUNDOS
// Display de 2 dígitos usando 2N3906 para activar los comunes
// S1 = START / STOP
// S2 = RESET

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"

// ==================== DEFINICIÓN DE PINES ====================
// Segmentos
#define SEG_A 4
#define SEG_B 16
#define SEG_C 17
#define SEG_D 18
#define SEG_E 19
#define SEG_F 21
#define SEG_G 2

// Dígitos controlados con transistores PNP 2N3906 - siempre por el display ser anodo común
#define DIG_DECENAS  25
#define DIG_UNIDADES 26

// Botones
#define BTN_S1 32
#define BTN_S2 33

// ==================== TIMER ==================== 
//siempre
#define TIMER_DIVIDER     80
#define TIMER_INTERVAL_US 5000   // 5 ms

// ==================== VARIABLES GLOBALES ====================
static volatile int segundos = 0;
static volatile bool cronometro_activo = false;

static volatile bool mostrar_decenas = true;
static volatile int acumulador_ms = 0;

static volatile bool estado_anterior_s1 = 1;
static volatile bool estado_anterior_s2 = 1;

static volatile int bloqueo_s1_ms = 0;
static volatile int bloqueo_s2_ms = 0;

// Tabla de dígitos
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

// ==================== CARGAR SEGMENTOS ====================
// Como es ánodo común, segmento encendido = 0 - por eso todo va negado
void cargar_segmentos(int numero) {
    gpio_set_level(SEG_A, !digitos[numero][0]);
    gpio_set_level(SEG_B, !digitos[numero][1]);
    gpio_set_level(SEG_C, !digitos[numero][2]);
    gpio_set_level(SEG_D, !digitos[numero][3]);
    gpio_set_level(SEG_E, !digitos[numero][4]);
    gpio_set_level(SEG_F, !digitos[numero][5]);
    gpio_set_level(SEG_G, !digitos[numero][6]);
}

// ==================== APAGAR DÍGITOS ====================
// Como usamos 2N3906, apagar dígito = GPIO en 1
void apagar_digitos(void) { //lo pongo en 1
    gpio_set_level(DIG_DECENAS, 1);
    gpio_set_level(DIG_UNIDADES, 1);
}

// ==================== REFRESCAR DISPLAY ====================
void refrescar_display(int valor) {
    int decenas = valor / 10;
    int unidades = valor % 10;

    apagar_digitos();

    if (mostrar_decenas) {
        cargar_segmentos(decenas);
        gpio_set_level(DIG_DECENAS, 0);   // activa transistor PNP
    } else {
        cargar_segmentos(unidades);
        gpio_set_level(DIG_UNIDADES, 0);  // activa transistor PNP
    }

    mostrar_decenas = !mostrar_decenas;
}

// ==================== ISR TIMER ====================
static bool IRAM_ATTR timer_isr(void *arg) {

    // Refrescar display por multiplexación
    refrescar_display(segundos);

    // Anti-rebote
    if (bloqueo_s1_ms > 0) {
        bloqueo_s1_ms -= 5;
    }

    if (bloqueo_s2_ms > 0) {
        bloqueo_s2_ms -= 5;
    }

    // Leer botones
    bool estado_actual_s1 = gpio_get_level(BTN_S1);
    bool estado_actual_s2 = gpio_get_level(BTN_S2);

    // START / STOP
    if ((estado_anterior_s1 == 1) && (estado_actual_s1 == 0) && (bloqueo_s1_ms == 0)) {
        cronometro_activo = !cronometro_activo;
        bloqueo_s1_ms = 50;
    }

    // RESET
    if ((estado_anterior_s2 == 1) && (estado_actual_s2 == 0) && (bloqueo_s2_ms == 0)) {
        segundos = 0;
        acumulador_ms = 0;
        bloqueo_s2_ms = 50;
    }

    estado_anterior_s1 = estado_actual_s1;
    estado_anterior_s2 = estado_actual_s2;

    // Contador de tiempo
    if (cronometro_activo) {
        acumulador_ms += 5;

        if (acumulador_ms >= 1000) {
            acumulador_ms = 0;
            segundos++;

            if (segundos > 59) {
                segundos = 0;
            }
        }
    }

    return false;
}

// ==================== MAIN ====================
void app_main() {

    // Configurar segmentos como salida
    gpio_config_t out_seg = {
        .pin_bit_mask =
            (1ULL << SEG_A) |
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

    gpio_config(&out_seg);

    // Configurar pines que controlan bases de transistores
    gpio_config_t out_dig = { // lo pongo como out cuando ya son más de dos
        .pin_bit_mask =
            (1ULL << DIG_DECENAS) |
            (1ULL << DIG_UNIDADES),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&out_dig);

    // Configurar botones
    gpio_config_t in_cfg = {
        .pin_bit_mask =
            (1ULL << BTN_S1) |
            (1ULL << BTN_S2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&in_cfg);

    apagar_digitos();
    cargar_segmentos(0);

    // Configuración del timer
    timer_config_t timer_cfg = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true
    };

    timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg);

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    timer_set_alarm_value(
        TIMER_GROUP_0,
        TIMER_0,
        TIMER_INTERVAL_US
    );

    timer_isr_callback_add(
        TIMER_GROUP_0,
        TIMER_0,
        timer_isr,
        NULL,
        0
    );

    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

    timer_start(TIMER_GROUP_0, TIMER_0);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
*/






/*

//////////////===============PUNTO 5======================
// PUNTO 5 - ALARMA CON CUENTA REGRESIVA DE 2 DÍGITOS
// Display de 2 dígitos con 2N3906 para los comunes
// S1 = INICIAR / PAUSAR / CONTINUAR
// S2 = SILENCIAR ALARMA Y REINICIAR
// Al llegar a 00, un LED rojo parpadea a 2 Hz

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"

// ==================== PINES ====================
// Segmentos
#define SEG_A 4
#define SEG_B 16
#define SEG_C 17
#define SEG_D 18
#define SEG_E 19
#define SEG_F 21
#define SEG_G 2

// Comunes del display con 2N3906
#define DIG_DECENAS  25
#define DIG_UNIDADES 26

// Botones
#define BTN_S1 32
#define BTN_S2 33

// LED de alarma
#define LED_ALARMA 27

// ==================== TIMER ====================
#define TIMER_DIVIDER     80
#define TIMER_INTERVAL_US 5000   // 5 ms

// ==================== CONFIGURACIÓN ==================== //yo lo elijo
#define VALOR_INICIAL 30   // entre 10 y 99

// ==================== VARIABLES GLOBALES ====================
static volatile int cuenta = VALOR_INICIAL;
static volatile bool temporizador_activo = false;
static volatile bool alarma_activa = false;

static volatile bool mostrar_decenas = true;
static volatile int acumulador_ms = 0;

static volatile bool estado_anterior_s1 = 1;
static volatile bool estado_anterior_s2 = 1;

static volatile int bloqueo_s1_ms = 0;
static volatile int bloqueo_s2_ms = 0;

// Para el parpadeo del LED a 2 Hz
static volatile int acumulador_alarma_ms = 0;
static volatile bool estado_led_alarma = false;

// Tabla de dígitos
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

// ==================== CARGAR SEGMENTOS ====================
// Display ánodo común -> segmento encendido = 0
void cargar_segmentos(int numero) {
    gpio_set_level(SEG_A, !digitos[numero][0]);
    gpio_set_level(SEG_B, !digitos[numero][1]);
    gpio_set_level(SEG_C, !digitos[numero][2]);
    gpio_set_level(SEG_D, !digitos[numero][3]);
    gpio_set_level(SEG_E, !digitos[numero][4]);
    gpio_set_level(SEG_F, !digitos[numero][5]);
    gpio_set_level(SEG_G, !digitos[numero][6]);
}

// ==================== APAGAR DÍGITOS ====================
// Con 2N3906: apagado = 1
void apagar_digitos(void) {
    gpio_set_level(DIG_DECENAS, 1);
    gpio_set_level(DIG_UNIDADES, 1);
}

// ==================== REFRESCAR DISPLAY ====================
void refrescar_display(int valor) {
    int decenas = valor / 10;
    int unidades = valor % 10;

    apagar_digitos();

    if (mostrar_decenas) {
        cargar_segmentos(decenas);
        gpio_set_level(DIG_DECENAS, 0);   // activar transistor PNP
    } else {
        cargar_segmentos(unidades);
        gpio_set_level(DIG_UNIDADES, 0);  // activar transistor PNP
    }

    mostrar_decenas = !mostrar_decenas;
}

// ==================== ISR TIMER ====================
static bool IRAM_ATTR timer_isr(void *arg) {

    // ---------- REFRESCAR DISPLAY ----------
    refrescar_display(cuenta);

    // ---------- ANTI-REBOTE ----------
    if (bloqueo_s1_ms > 0) {
        bloqueo_s1_ms -= 5;
    }

    if (bloqueo_s2_ms > 0) {
        bloqueo_s2_ms -= 5;
    }

    // ---------- LEER BOTONES ----------
    bool estado_actual_s1 = gpio_get_level(BTN_S1);
    bool estado_actual_s2 = gpio_get_level(BTN_S2);

    // ---------- S1: INICIAR / PAUSAR / CONTINUAR ----------
    if ((estado_anterior_s1 == 1) && (estado_actual_s1 == 0) && (bloqueo_s1_ms == 0)) {

        // Solo cambia el estado si la alarma no está activa
        if (!alarma_activa) {
            temporizador_activo = !temporizador_activo;
        }

        bloqueo_s1_ms = 50;
    }

    // ---------- S2: SILENCIAR ALARMA Y REINICIAR ----------
    if ((estado_anterior_s2 == 1) && (estado_actual_s2 == 0) && (bloqueo_s2_ms == 0)) {
        cuenta = VALOR_INICIAL;
        temporizador_activo = false;
        alarma_activa = false;
        acumulador_ms = 0;
        acumulador_alarma_ms = 0;
        estado_led_alarma = false;
        gpio_set_level(LED_ALARMA, 0);
        bloqueo_s2_ms = 50;
    }

    estado_anterior_s1 = estado_actual_s1;
    estado_anterior_s2 = estado_actual_s2;

    // ---------- CUENTA REGRESIVA ----------
    if (temporizador_activo && !alarma_activa) {
        acumulador_ms += 5;

        if (acumulador_ms >= 1000) {
            acumulador_ms = 0;
            cuenta--;

            if (cuenta <= 0) {
                cuenta = 0;
                temporizador_activo = false;
                alarma_activa = true;
            }
        }
    }

    // ---------- PARPADEO DE ALARMA A 2 Hz ----------
    if (alarma_activa) {
        acumulador_alarma_ms += 5;

        // Cambia cada 250 ms -> parpadeo total de 500 ms -> 2 Hz
        if (acumulador_alarma_ms >= 250) {
            acumulador_alarma_ms = 0;
            estado_led_alarma = !estado_led_alarma;
            gpio_set_level(LED_ALARMA, estado_led_alarma);
        }
    }

    return false;
}

// ==================== MAIN ====================
void app_main() {

    // Configurar segmentos como salida
    gpio_config_t out_seg = {
        .pin_bit_mask =
            (1ULL << SEG_A) |
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
    gpio_config(&out_seg);

    // Configurar control de dígitos como salida
    gpio_config_t out_dig = {
        .pin_bit_mask =
            (1ULL << DIG_DECENAS) |
            (1ULL << DIG_UNIDADES),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&out_dig);

    // Configurar LED de alarma como salida
    gpio_config_t out_alarm = {
        .pin_bit_mask = (1ULL << LED_ALARMA),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&out_alarm);

    // Configurar botones como entrada
    gpio_config_t in_cfg = {
        .pin_bit_mask =
            (1ULL << BTN_S1) |
            (1ULL << BTN_S2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&in_cfg);

    // Estado inicial
    apagar_digitos();
    cargar_segmentos(cuenta);
    gpio_set_level(LED_ALARMA, 0);

    // Configuración del timer
    timer_config_t timer_cfg = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true
    };

    timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    timer_set_alarm_value(
        TIMER_GROUP_0,
        TIMER_0,
        TIMER_INTERVAL_US
    );

    timer_isr_callback_add(
        TIMER_GROUP_0,
        TIMER_0,
        timer_isr,
        NULL,
        0
    );

    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_start(TIMER_GROUP_0, TIMER_0);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


*/






/*

//===========INTENTO PARCIAL ===============================
// EJERCICIO ASCENSOR 4 PISOS
// Display 1 = piso actual
// Display 2 = piso destino
// 4 botones, uno por piso
// 1 LED verde
// Display de 4 dígitos ánodo común usando 2 dígitos y 2 transistores 2N3906
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"
// ==================== PINES ====================
// Segmentos
#define SEG_A 4
#define SEG_B 16
#define SEG_C 17
#define SEG_D 18
#define SEG_E 19
#define SEG_F 21
#define SEG_G 22
// Comunes de los dos displays (controlados con 2N3906)
#define DIG_DISPLAY1 25
#define DIG_DISPLAY2 26
// LED verde
#define LED_VERDE 27
// Botones de pisos
#define BTN_P1 32
#define BTN_P2 33
#define BTN_P3 23
#define BTN_P4 13
// ==================== TIMER ====================
#define TIMER_DIVIDER     80
#define TIMER_INTERVAL_US 5000   // 5 ms
// ==================== ESTADOS ====================
typedef enum {
   ESPERA,
   MOVIMIENTO,
   LLEGADA
} estado_ascensor_t;

// ==================== VARIABLES GLOBALES ====================
// Estado del sistema
static volatile estado_ascensor_t estado = ESPERA;
// Piso actual y destino
static volatile int piso_actual = 1;
static volatile int piso_destino = 1;
// Valores a mostrar en los displays
// -1 significa guion "--"
static volatile int display1_valor = -1;
static volatile int display2_valor = -1;
// Multiplexación
static volatile bool mostrar_display1 = true;
// Temporizadores
static volatile int acumulador_mov_ms = 0;
static volatile int acumulador_llegada_ms = 0;
static volatile int acumulador_parpadeo_ms = 0;
// Estado del LED verde
static volatile bool led_verde_estado = false;
// Estado anterior de botones
static volatile bool estado_anterior_p1 = 1;
static volatile bool estado_anterior_p2 = 1;
static volatile bool estado_anterior_p3 = 1;
static volatile bool estado_anterior_p4 = 1;
// Anti-rebote
static volatile int bloqueo_p1_ms = 0;
static volatile int bloqueo_p2_ms = 0;
static volatile int bloqueo_p3_ms = 0;
static volatile int bloqueo_p4_ms = 0;
// Tabla de dígitos
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
// ==================== CARGAR SEGMENTOS ====================
// Como el display es ánodo común, segmento encendido = 0
void cargar_segmentos(int numero) {
   // Si numero = -1, mostrar guion
   if (numero == -1) {
       gpio_set_level(SEG_A, 1);
       gpio_set_level(SEG_B, 1);
       gpio_set_level(SEG_C, 1);
       gpio_set_level(SEG_D, 1);
       gpio_set_level(SEG_E, 1);
       gpio_set_level(SEG_F, 1);
       gpio_set_level(SEG_G, 0);   // solo g encendido
       return;
   }
   gpio_set_level(SEG_A, !digitos[numero][0]);
   gpio_set_level(SEG_B, !digitos[numero][1]);
   gpio_set_level(SEG_C, !digitos[numero][2]);
   gpio_set_level(SEG_D, !digitos[numero][3]);
   gpio_set_level(SEG_E, !digitos[numero][4]);
   gpio_set_level(SEG_F, !digitos[numero][5]);
   gpio_set_level(SEG_G, !digitos[numero][6]);
}
// ==================== APAGAR DÍGITOS ====================
// Con 2N3906: apagado = 1
void apagar_digitos(void) {
   gpio_set_level(DIG_DISPLAY1, 1);
   gpio_set_level(DIG_DISPLAY2, 1);
}
// ==================== REFRESCAR DISPLAYS ====================
void refrescar_displays(void) {
   apagar_digitos();
   if (mostrar_display1) {
       cargar_segmentos(display1_valor);
       gpio_set_level(DIG_DISPLAY1, 0);   // activar transistor PNP
   } else {
       cargar_segmentos(display2_valor);
       gpio_set_level(DIG_DISPLAY2, 0);   // activar transistor PNP
   }
   mostrar_display1 = !mostrar_display1;
}
// ==================== ISR TIMER ====================
static bool IRAM_ATTR timer_isr(void *arg) {
   // ----------- MULTIPLEXACIÓN -----------
   refrescar_displays();
   // ----------- ANTI-REBOTE -----------
   if (bloqueo_p1_ms > 0) bloqueo_p1_ms -= 5;
   if (bloqueo_p2_ms > 0) bloqueo_p2_ms -= 5;
   if (bloqueo_p3_ms > 0) bloqueo_p3_ms -= 5;
   if (bloqueo_p4_ms > 0) bloqueo_p4_ms -= 5;
   // ----------- LEER BOTONES -----------
   bool estado_actual_p1 = gpio_get_level(BTN_P1);
   bool estado_actual_p2 = gpio_get_level(BTN_P2);
   bool estado_actual_p3 = gpio_get_level(BTN_P3);
   bool estado_actual_p4 = gpio_get_level(BTN_P4);
   // Solo se atienden botones en ESPERA
   if (estado == ESPERA) {
       // Piso 1
       if ((estado_anterior_p1 == 1) && (estado_actual_p1 == 0) && (bloqueo_p1_ms == 0)) {
           if (piso_actual != 1) {
               piso_destino = 1;
               display1_valor = piso_actual;
               display2_valor = piso_destino;
               estado = MOVIMIENTO;
               acumulador_mov_ms = 0;
           }
           bloqueo_p1_ms = 50;
       }
       // Piso 2
       if ((estado_anterior_p2 == 1) && (estado_actual_p2 == 0) && (bloqueo_p2_ms == 0)) {
           if (piso_actual != 2) {
               piso_destino = 2;
               display1_valor = piso_actual;
               display2_valor = piso_destino;
               estado = MOVIMIENTO;
               acumulador_mov_ms = 0;
           }
           bloqueo_p2_ms = 50;
       }
       // Piso 3
       if ((estado_anterior_p3 == 1) && (estado_actual_p3 == 0) && (bloqueo_p3_ms == 0)) {
           if (piso_actual != 3) {
               piso_destino = 3;
               display1_valor = piso_actual;
               display2_valor = piso_destino;
               estado = MOVIMIENTO;
               acumulador_mov_ms = 0;
           }
           bloqueo_p3_ms = 50;
       }
       // Piso 4
       if ((estado_anterior_p4 == 1) && (estado_actual_p4 == 0) && (bloqueo_p4_ms == 0)) {
           if (piso_actual != 4) {
               piso_destino = 4;
               display1_valor = piso_actual;
               display2_valor = piso_destino;
               estado = MOVIMIENTO;
               acumulador_mov_ms = 0;
           }
           bloqueo_p4_ms = 50;
       }
   }
   // Guardar estados de botones
   estado_anterior_p1 = estado_actual_p1;
   estado_anterior_p2 = estado_actual_p2;
   estado_anterior_p3 = estado_actual_p3;
   estado_anterior_p4 = estado_actual_p4;
   // ----------- ESTADO MOVIMIENTO -----------
   if (estado == MOVIMIENTO) {
       acumulador_mov_ms += 5;
       // Cada 1 segundo el ascensor cambia de piso
       if (acumulador_mov_ms >= 1000) {
           acumulador_mov_ms = 0;
           if (piso_actual < piso_destino) {
               piso_actual++;
           } else if (piso_actual > piso_destino) {
               piso_actual--;
           }
           display1_valor = piso_actual;
           display2_valor = piso_destino;
           // Si llegó al destino
           if (piso_actual == piso_destino) {
               estado = LLEGADA;
               acumulador_llegada_ms = 0;
               acumulador_parpadeo_ms = 0;
               led_verde_estado = false;
               gpio_set_level(LED_VERDE, 0);
               display1_valor = piso_destino;
               display2_valor = piso_destino;
           }
       }
   }
   // ----------- ESTADO LLEGADA -----------
   else if (estado == LLEGADA) {
       acumulador_llegada_ms += 5;
       acumulador_parpadeo_ms += 5;
       // Parpadeo a 2 Hz -> cambia cada 250 ms
       if (acumulador_parpadeo_ms >= 250) {
           acumulador_parpadeo_ms = 0;
           led_verde_estado = !led_verde_estado;
           gpio_set_level(LED_VERDE, led_verde_estado);
       }
       // Mantener esta condición por 5 segundos
       if (acumulador_llegada_ms >= 5000) {
           estado = ESPERA;
           display1_valor = -1;
           display2_valor = -1;
           gpio_set_level(LED_VERDE, 0);
       }
   }
   return false;
}
// ==================== MAIN ====================
void app_main() {
   // ----------- CONFIGURAR SEGMENTOS COMO SALIDA -----------
   gpio_config_t out_seg = {
       .pin_bit_mask =
           (1ULL << SEG_A) |
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
   gpio_config(&out_seg);
   // ----------- CONFIGURAR CONTROL DE DÍGITOS COMO SALIDA -----------
   gpio_config_t out_dig = {
       .pin_bit_mask =
           (1ULL << DIG_DISPLAY1) |
           (1ULL << DIG_DISPLAY2),
       .mode = GPIO_MODE_OUTPUT,
       .pull_up_en = GPIO_PULLUP_DISABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&out_dig);
   // ----------- CONFIGURAR LED VERDE COMO SALIDA -----------
   gpio_config_t out_led = {
       .pin_bit_mask = (1ULL << LED_VERDE),
       .mode = GPIO_MODE_OUTPUT,
       .pull_up_en = GPIO_PULLUP_DISABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&out_led);
   // ----------- CONFIGURAR BOTONES COMO ENTRADA -----------
   gpio_config_t in_cfg = {
       .pin_bit_mask =
           (1ULL << BTN_P1) |
           (1ULL << BTN_P2) |
           (1ULL << BTN_P3) |
           (1ULL << BTN_P4),
       .mode = GPIO_MODE_INPUT,
       .pull_up_en = GPIO_PULLUP_ENABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&in_cfg);
   // Estado inicial
   display1_valor = -1;
   display2_valor = -1;
   gpio_set_level(LED_VERDE, 0);
   // ----------- CONFIGURACIÓN DEL TIMER -----------
   timer_config_t timer_cfg = {
       .divider = TIMER_DIVIDER,
       .counter_dir = TIMER_COUNT_UP,
       .counter_en = TIMER_PAUSE,
       .alarm_en = TIMER_ALARM_EN,
       .auto_reload = true
   };
   timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg);
   timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
   timer_set_alarm_value(
       TIMER_GROUP_0,
       TIMER_0,
       TIMER_INTERVAL_US
   );
   timer_isr_callback_add(
       TIMER_GROUP_0,
       TIMER_0,
       timer_isr,
       NULL,
       0
   );
   timer_enable_intr(TIMER_GROUP_0, TIMER_0);
   timer_start(TIMER_GROUP_0, TIMER_0);
   while (1) {
       vTaskDelay(pdMS_TO_TICKS(1));
   }
}

*/





//
// EJERCICIO PROPUESTO - TEMPORIZADOR DE ESTACIONAMIENTO 000 A 180 s

// 3 dígitos de display ánodo común con 2N3906

// S1 = aumentar 10 s

// S2 = disminuir 10 s

// S3 = iniciar / pausar / continuar

// LED rojo = alarma a 2 Hz cuando llega a 000

#include <stdio.h>

#include <stdbool.h>

#include "freertos/FreeRTOS.h"

#include "freertos/task.h"

#include "driver/gpio.h"

#include "driver/timer.h"

// ==================== PINES ====================

// Segmentos

#define SEG_A 4

#define SEG_B 16

#define SEG_C 17

#define SEG_D 18

#define SEG_E 19

#define SEG_F 21

#define SEG_G 22

// Dígitos (controlados con 2N3906)

#define DIG_CENTENAS 25

#define DIG_DECENAS  26

#define DIG_UNIDADES 27

// Botones

#define BTN_S1 32

#define BTN_S2 33

#define BTN_S3 35

// LED alarma

#define LED_ALARMA 23

// ==================== TIMER ====================

#define TIMER_DIVIDER     80

#define TIMER_INTERVAL_US 5000   // 5 ms

// ==================== LÍMITES ====================

#define TIEMPO_MIN 0

#define TIEMPO_MAX 180

// ==================== ESTADOS ====================

typedef enum {

    ESTADO_CONFIG = 0,

    ESTADO_CORRIENDO,

    ESTADO_PAUSA,

    ESTADO_ALARMA

} estado_t;

// ==================== VARIABLES GLOBALES ====================

static volatile estado_t estado = ESTADO_CONFIG;

static volatile int tiempo = 0;            // tiempo mostrado en segundos

static volatile int acumulador_ms = 0;     // para contar 1 segundo

// Multiplexación

static volatile int digito_activo = 0;

// Parpadeo alarma

static volatile int acumulador_alarma_ms = 0;

static volatile bool led_alarma_estado = false;

// Estado anterior botones

static volatile bool estado_anterior_s1 = 1;

static volatile bool estado_anterior_s2 = 1;

static volatile bool estado_anterior_s3 = 1;

// Anti-rebote

static volatile int bloqueo_s1_ms = 0;

static volatile int bloqueo_s2_ms = 0;

static volatile int bloqueo_s3_ms = 0;

// Tabla de números

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

// ==================== SEGMENTOS ====================

// Display ánodo común -> segmento encendido = 0

void cargar_segmentos(int numero) {

    gpio_set_level(SEG_A, !digitos[numero][0]);

    gpio_set_level(SEG_B, !digitos[numero][1]);

    gpio_set_level(SEG_C, !digitos[numero][2]);

    gpio_set_level(SEG_D, !digitos[numero][3]);

    gpio_set_level(SEG_E, !digitos[numero][4]);

    gpio_set_level(SEG_F, !digitos[numero][5]);

    gpio_set_level(SEG_G, !digitos[numero][6]);

}

// ==================== APAGAR DÍGITOS ====================

// Con transistor PNP 2N3906: apagado = GPIO en 1

void apagar_digitos(void) {

    gpio_set_level(DIG_CENTENAS, 1);

    gpio_set_level(DIG_DECENAS, 1);

    gpio_set_level(DIG_UNIDADES, 1);

}

// ==================== REFRESCAR DISPLAY ====================

void refrescar_display(int valor) {

    int centenas = valor / 100;

    int decenas  = (valor / 10) % 10;

    int unidades = valor % 10;

    apagar_digitos();

    if (digito_activo == 0) {

        cargar_segmentos(centenas);

        gpio_set_level(DIG_CENTENAS, 0);   // activa transistor PNP

    }

    else if (digito_activo == 1) {

        cargar_segmentos(decenas);

        gpio_set_level(DIG_DECENAS, 0);

    }

    else {

        cargar_segmentos(unidades);

        gpio_set_level(DIG_UNIDADES, 0);

    }

    digito_activo++;

    if (digito_activo > 2) {

        digito_activo = 0;

    }

}

// ==================== ISR TIMER ====================

static bool IRAM_ATTR timer_isr(void *arg) {

    // ----------- REFRESCAR DISPLAY -----------

    refrescar_display(tiempo);

    // ----------- ANTI-REBOTE -----------

    if (bloqueo_s1_ms > 0) bloqueo_s1_ms -= 5;

    if (bloqueo_s2_ms > 0) bloqueo_s2_ms -= 5;

    if (bloqueo_s3_ms > 0) bloqueo_s3_ms -= 5;

    // ----------- LEER BOTONES -----------

    bool estado_actual_s1 = gpio_get_level(BTN_S1);

    bool estado_actual_s2 = gpio_get_level(BTN_S2);

    bool estado_actual_s3 = gpio_get_level(BTN_S3);

    // ==================== ESTADO CONFIG ====================

    if (estado == ESTADO_CONFIG) {

        // S1 aumenta 10 s

        if ((estado_anterior_s1 == 1) && (estado_actual_s1 == 0) && (bloqueo_s1_ms == 0)) {

            tiempo += 10;

            if (tiempo > TIEMPO_MAX) {

                tiempo = TIEMPO_MAX;

            }

            bloqueo_s1_ms = 50;

        }

        // S2 disminuye 10 s

        if ((estado_anterior_s2 == 1) && (estado_actual_s2 == 0) && (bloqueo_s2_ms == 0)) {

            tiempo -= 10;

            if (tiempo < TIEMPO_MIN) {

                tiempo = TIEMPO_MIN;

            }

            bloqueo_s2_ms = 50;

        }

        // S3 inicia

        if ((estado_anterior_s3 == 1) && (estado_actual_s3 == 0) && (bloqueo_s3_ms == 0)) {

            if (tiempo > 0) {

                estado = ESTADO_CORRIENDO;

                acumulador_ms = 0;

            }

            bloqueo_s3_ms = 50;

        }

    }

    // ==================== ESTADO CORRIENDO ====================

    else if (estado == ESTADO_CORRIENDO) {

        // S3 pausa

        if ((estado_anterior_s3 == 1) && (estado_actual_s3 == 0) && (bloqueo_s3_ms == 0)) {

            estado = ESTADO_PAUSA;

            bloqueo_s3_ms = 50;

        }

        acumulador_ms += 5;

        if (acumulador_ms >= 1000) {

            acumulador_ms = 0;

            tiempo--;

            if (tiempo <= 0) {

                tiempo = 0;

                estado = ESTADO_ALARMA;

                acumulador_alarma_ms = 0;

                led_alarma_estado = false;

                gpio_set_level(LED_ALARMA, 0);

            }

        }

    }

    // ==================== ESTADO PAUSA ====================

    else if (estado == ESTADO_PAUSA) {

        // S3 continúa

        if ((estado_anterior_s3 == 1) && (estado_actual_s3 == 0) && (bloqueo_s3_ms == 0)) {

            estado = ESTADO_CORRIENDO;

            bloqueo_s3_ms = 50;

        }

    }

    // ==================== ESTADO ALARMA ====================

    else if (estado == ESTADO_ALARMA) {

        // Parpadeo a 2 Hz: cambio cada 250 ms

        acumulador_alarma_ms += 5;

        if (acumulador_alarma_ms >= 250) {

            acumulador_alarma_ms = 0;

            led_alarma_estado = !led_alarma_estado;

            gpio_set_level(LED_ALARMA, led_alarma_estado);

        }

        // S1 apaga alarma y vuelve a 000

        if ((estado_anterior_s1 == 1) && (estado_actual_s1 == 0) && (bloqueo_s1_ms == 0)) {

            tiempo = 0;

            estado = ESTADO_CONFIG;

            acumulador_ms = 0;

            acumulador_alarma_ms = 0;

            led_alarma_estado = false;

            gpio_set_level(LED_ALARMA, 0);

            bloqueo_s1_ms = 50;

        }

    }

    // Guardar estados para próxima comparación

    estado_anterior_s1 = estado_actual_s1;

    estado_anterior_s2 = estado_actual_s2;

    estado_anterior_s3 = estado_actual_s3;

    return false;

}

// ==================== MAIN ====================

void app_main() {

    // Configurar segmentos como salida

    gpio_config_t out_seg = {

        .pin_bit_mask =

            (1ULL << SEG_A) |

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

    gpio_config(&out_seg);

    // Configurar pines de control de dígitos como salida

    gpio_config_t out_dig = {

        .pin_bit_mask =

            (1ULL << DIG_CENTENAS) |

            (1ULL << DIG_DECENAS) |

            (1ULL << DIG_UNIDADES),

        .mode = GPIO_MODE_OUTPUT,

        .pull_up_en = GPIO_PULLUP_DISABLE,

        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE

    };

    gpio_config(&out_dig);

    // Configurar LED alarma como salida

    gpio_config_t out_led = {

        .pin_bit_mask = (1ULL << LED_ALARMA),

        .mode = GPIO_MODE_OUTPUT,

        .pull_up_en = GPIO_PULLUP_DISABLE,

        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE

    };

    gpio_config(&out_led);

    // Configurar botones como entrada

    gpio_config_t in_cfg = {

        .pin_bit_mask =

            (1ULL << BTN_S1) |

            (1ULL << BTN_S2) |

            (1ULL << BTN_S3),

        .mode = GPIO_MODE_INPUT,

        .pull_up_en = GPIO_PULLUP_ENABLE,

        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE

    };

    gpio_config(&in_cfg);

    // Estado inicial

    apagar_digitos();

    cargar_segmentos(0);

    gpio_set_level(LED_ALARMA, 0);

    // Configurar timer

    timer_config_t timer_cfg = {

        .divider = TIMER_DIVIDER,

        .counter_dir = TIMER_COUNT_UP,

        .counter_en = TIMER_PAUSE,

        .alarm_en = TIMER_ALARM_EN,

        .auto_reload = true

    };

    timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg);

    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    timer_set_alarm_value(

        TIMER_GROUP_0,

        TIMER_0,

        TIMER_INTERVAL_US

    );

    timer_isr_callback_add(

        TIMER_GROUP_0,

        TIMER_0,

        timer_isr,

        NULL,

        0

    );

    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

    timer_start(TIMER_GROUP_0, TIMER_0);

    while (1) {

        vTaskDelay(pdMS_TO_TICKS(1));

    }

}
 







/*


// EJERCICIO PROPUESTO - SISTEMA DE LLAMADA DE TURNOS
// Display de 4 dígitos ánodo común con 4 transistores 2N3906
// S1 = siguiente turno
// S2 = reiniciar a 0001
// S3 = activar / desactivar modo llamada
// LED verde parpadea a 2 Hz en modo llamada
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"
// ==================== PINES ====================
// Segmentos
#define SEG_A 4
#define SEG_B 16
#define SEG_C 17
#define SEG_D 18
#define SEG_E 19
#define SEG_F 21
#define SEG_G 22
// Dígitos (con 2N3906)
#define DIG_1 25
#define DIG_2 26
#define DIG_3 27
#define DIG_4 14
// Botones
#define BTN_S1 32
#define BTN_S2 33
#define BTN_S3 23
// LED
#define LED_VERDE 13
// ==================== TIMER ====================
#define TIMER_DIVIDER     80
#define TIMER_INTERVAL_US 5000   // 5 ms
// ==================== VARIABLES GLOBALES ====================
static volatile int turno = 1;
// Para multiplexación
static volatile int digito_activo = 0;
// Para parpadeo del LED
static volatile bool modo_llamada = false;
static volatile bool estado_led = false;
static volatile int acumulador_led_ms = 0;
// Estado anterior de botones
static volatile bool estado_anterior_s1 = 1;
static volatile bool estado_anterior_s2 = 1;
static volatile bool estado_anterior_s3 = 1;
// Anti-rebote
static volatile int bloqueo_s1_ms = 0;
static volatile int bloqueo_s2_ms = 0;
static volatile int bloqueo_s3_ms = 0;
// Tabla de números
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
// ==================== CARGAR SEGMENTOS ====================
// Display ánodo común -> segmento encendido = 0
void cargar_segmentos(int numero) {
   gpio_set_level(SEG_A, !digitos[numero][0]);
   gpio_set_level(SEG_B, !digitos[numero][1]);
   gpio_set_level(SEG_C, !digitos[numero][2]);
   gpio_set_level(SEG_D, !digitos[numero][3]);
   gpio_set_level(SEG_E, !digitos[numero][4]);
   gpio_set_level(SEG_F, !digitos[numero][5]);
   gpio_set_level(SEG_G, !digitos[numero][6]);
}
// ==================== APAGAR TODOS LOS DÍGITOS ====================
// Con 2N3906: apagado = 1
void apagar_digitos(void) {
   gpio_set_level(DIG_1, 1);
   gpio_set_level(DIG_2, 1);
   gpio_set_level(DIG_3, 1);
   gpio_set_level(DIG_4, 1);
}
// ==================== REFRESCAR DISPLAY ====================
void refrescar_display(int valor) {
   int d1 = (valor / 1000) % 10;
   int d2 = (valor / 100) % 10;
   int d3 = (valor / 10) % 10;
   int d4 = valor % 10;
   apagar_digitos();
   if (digito_activo == 0) {
       cargar_segmentos(d1);
       gpio_set_level(DIG_1, 0);
   }
   else if (digito_activo == 1) {
       cargar_segmentos(d2);
       gpio_set_level(DIG_2, 0);
   }
   else if (digito_activo == 2) {
       cargar_segmentos(d3);
       gpio_set_level(DIG_3, 0);
   }
   else {
       cargar_segmentos(d4);
       gpio_set_level(DIG_4, 0);
   }
   digito_activo++;
   if (digito_activo > 3) {
       digito_activo = 0;
   }
}
// ==================== ISR TIMER ====================
static bool IRAM_ATTR timer_isr(void *arg) {
   // Multiplexación del display
   refrescar_display(turno);
   // Anti-rebote
   if (bloqueo_s1_ms > 0) bloqueo_s1_ms -= 5;
   if (bloqueo_s2_ms > 0) bloqueo_s2_ms -= 5;
   if (bloqueo_s3_ms > 0) bloqueo_s3_ms -= 5;
   // Leer botones
   bool estado_actual_s1 = gpio_get_level(BTN_S1);
   bool estado_actual_s2 = gpio_get_level(BTN_S2);
   bool estado_actual_s3 = gpio_get_level(BTN_S3);
   // S1 = siguiente turno
   if ((estado_anterior_s1 == 1) && (estado_actual_s1 == 0) && (bloqueo_s1_ms == 0)) {
       turno++;
       if (turno > 9999) {
           turno = 1;
       }
       bloqueo_s1_ms = 50;
   }
   // S2 = reiniciar turno
   if ((estado_anterior_s2 == 1) && (estado_actual_s2 == 0) && (bloqueo_s2_ms == 0)) {
       turno = 1;
       bloqueo_s2_ms = 50;
   }
   // S3 = activar/desactivar modo llamada
   if ((estado_anterior_s3 == 1) && (estado_actual_s3 == 0) && (bloqueo_s3_ms == 0)) {
       modo_llamada = !modo_llamada;
       if (!modo_llamada) {
           estado_led = false;
           gpio_set_level(LED_VERDE, 0);
           acumulador_led_ms = 0;
       }
       bloqueo_s3_ms = 50;
   }
   estado_anterior_s1 = estado_actual_s1;
   estado_anterior_s2 = estado_actual_s2;
   estado_anterior_s3 = estado_actual_s3;
   // Parpadeo del LED a 2 Hz
   if (modo_llamada) {
       acumulador_led_ms += 5;
       if (acumulador_led_ms >= 250) {
           acumulador_led_ms = 0;
           estado_led = !estado_led;
           gpio_set_level(LED_VERDE, estado_led);
       }
   }
   return false;
}
// ==================== MAIN ====================
void app_main() {
   // Configurar segmentos como salida
   gpio_config_t out_seg = {
       .pin_bit_mask =
           (1ULL << SEG_A) |
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
   gpio_config(&out_seg);
   // Configurar pines de dígitos como salida
   gpio_config_t out_dig = {
       .pin_bit_mask =
           (1ULL << DIG_1) |
           (1ULL << DIG_2) |
           (1ULL << DIG_3) |
           (1ULL << DIG_4),
       .mode = GPIO_MODE_OUTPUT,
       .pull_up_en = GPIO_PULLUP_DISABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&out_dig);
   // Configurar LED como salida
   gpio_config_t out_led = {
       .pin_bit_mask = (1ULL << LED_VERDE),
       .mode = GPIO_MODE_OUTPUT,
       .pull_up_en = GPIO_PULLUP_DISABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&out_led);
   // Configurar botones como entrada
   gpio_config_t in_cfg = {
       .pin_bit_mask =
           (1ULL << BTN_S1) |
           (1ULL << BTN_S2) |
           (1ULL << BTN_S3),
       .mode = GPIO_MODE_INPUT,
       .pull_up_en = GPIO_PULLUP_ENABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&in_cfg);
   // Estado inicial
   apagar_digitos();
   cargar_segmentos(0);
   gpio_set_level(LED_VERDE, 0);
   // Configurar timer
   timer_config_t timer_cfg = {
       .divider = TIMER_DIVIDER,
       .counter_dir = TIMER_COUNT_UP,
       .counter_en = TIMER_PAUSE,
       .alarm_en = TIMER_ALARM_EN,
       .auto_reload = true
   };
   timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg);
   timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
   timer_set_alarm_value(
       TIMER_GROUP_0,
       TIMER_0,
       TIMER_INTERVAL_US
   );
   timer_isr_callback_add(
       TIMER_GROUP_0,
       TIMER_0,
       timer_isr,
       NULL,
       0
   );
   timer_enable_intr(TIMER_GROUP_0, TIMER_0);
   timer_start(TIMER_GROUP_0, TIMER_0);
   while (1) {
       vTaskDelay(pdMS_TO_TICKS(1));
   }
}
*/







/*

// EJERCICIO PROPUESTO - MARCADOR LOCAL / VISITANTE
// Display de 4 dígitos ánodo común con 4 transistores 2N3906
// S1 = +1 local
// S2 = +1 visitante
// S3 = reset 00-00
// S4 = activar/desactivar fin de partido
// LED rojo parpadea a 2 Hz en fin de partido
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"
// ==================== PINES ====================
// Segmentos
#define SEG_A 4
#define SEG_B 16
#define SEG_C 17
#define SEG_D 18
#define SEG_E 19
#define SEG_F 21
#define SEG_G 22
// Dígitos (con 2N3906)
#define DIG_1 25
#define DIG_2 26
#define DIG_3 27
#define DIG_4 14
// Botones
#define BTN_S1 32   // +1 local
#define BTN_S2 33   // +1 visitante
#define BTN_S3 23   // reset
#define BTN_S4 13   // fin de partido
// LED rojo
#define LED_ROJO 5
// ==================== TIMER ====================
#define TIMER_DIVIDER     80
#define TIMER_INTERVAL_US 5000   // 5 ms
// ==================== VARIABLES GLOBALES ====================
static volatile int marcador_local = 0;
static volatile int marcador_visitante = 0;
// Multiplexación
static volatile int digito_activo = 0;
// Estado de fin de partido
static volatile bool fin_partido = false;
// Parpadeo LED
static volatile bool estado_led = false;
static volatile int acumulador_led_ms = 0;
// Estado anterior de botones
static volatile bool estado_anterior_s1 = 1;
static volatile bool estado_anterior_s2 = 1;
static volatile bool estado_anterior_s3 = 1;
static volatile bool estado_anterior_s4 = 1;
// Anti-rebote
static volatile int bloqueo_s1_ms = 0;
static volatile int bloqueo_s2_ms = 0;
static volatile int bloqueo_s3_ms = 0;
static volatile int bloqueo_s4_ms = 0;
// Tabla de dígitos
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
// ==================== CARGAR SEGMENTOS ====================
// Display ánodo común -> segmento encendido = 0
void cargar_segmentos(int numero) {
   gpio_set_level(SEG_A, !digitos[numero][0]);
   gpio_set_level(SEG_B, !digitos[numero][1]);
   gpio_set_level(SEG_C, !digitos[numero][2]);
   gpio_set_level(SEG_D, !digitos[numero][3]);
   gpio_set_level(SEG_E, !digitos[numero][4]);
   gpio_set_level(SEG_F, !digitos[numero][5]);
   gpio_set_level(SEG_G, !digitos[numero][6]);
}
// ==================== APAGAR TODOS LOS DÍGITOS ====================
// Con 2N3906: apagado = 1
void apagar_digitos(void) {
   gpio_set_level(DIG_1, 1);
   gpio_set_level(DIG_2, 1);
   gpio_set_level(DIG_3, 1);
   gpio_set_level(DIG_4, 1);
}
// ==================== REFRESCAR DISPLAY ====================
void refrescar_display(void) {
   int local_decenas = marcador_local / 10;
   int local_unidades = marcador_local % 10;
   int vis_decenas = marcador_visitante / 10;
   int vis_unidades = marcador_visitante % 10;
   apagar_digitos();
   if (digito_activo == 0) {
       cargar_segmentos(local_decenas);
       gpio_set_level(DIG_1, 0);
   }
   else if (digito_activo == 1) {
       cargar_segmentos(local_unidades);
       gpio_set_level(DIG_2, 0);
   }
   else if (digito_activo == 2) {
       cargar_segmentos(vis_decenas);
       gpio_set_level(DIG_3, 0);
   }
   else {
       cargar_segmentos(vis_unidades);
       gpio_set_level(DIG_4, 0);
   }
   digito_activo++;
   if (digito_activo > 3) {
       digito_activo = 0;
   }
}

// ==================== ISR TIMER ====================
static bool IRAM_ATTR timer_isr(void *arg) {

   // Multiplexación
   refrescar_display();

   
   // Anti-rebote
   if (bloqueo_s1_ms > 0) bloqueo_s1_ms -= 5;
   if (bloqueo_s2_ms > 0) bloqueo_s2_ms -= 5;
   if (bloqueo_s3_ms > 0) bloqueo_s3_ms -= 5;
   if (bloqueo_s4_ms > 0) bloqueo_s4_ms -= 5;
   // Leer botones
   bool estado_actual_s1 = gpio_get_level(BTN_S1);
   bool estado_actual_s2 = gpio_get_level(BTN_S2);
   bool estado_actual_s3 = gpio_get_level(BTN_S3);
   bool estado_actual_s4 = gpio_get_level(BTN_S4);
   // S1 = +1 local
   if ((estado_anterior_s1 == 1) && (estado_actual_s1 == 0) && (bloqueo_s1_ms == 0)) {
       if (!fin_partido) {
           if (marcador_local < 99) {
               marcador_local++;
           }
       }
       bloqueo_s1_ms = 50;
   }
   // S2 = +1 visitante
   if ((estado_anterior_s2 == 1) && (estado_actual_s2 == 0) && (bloqueo_s2_ms == 0)) {
       if (!fin_partido) {
           if (marcador_visitante < 99) {
               marcador_visitante++;
           }
       }
       bloqueo_s2_ms = 50;
   }
   // S3 = reset
   if ((estado_anterior_s3 == 1) && (estado_actual_s3 == 0) && (bloqueo_s3_ms == 0)) {
       marcador_local = 0;
       marcador_visitante = 0;
       bloqueo_s3_ms = 50;
   }
   // S4 = activar/desactivar fin de partido
   if ((estado_anterior_s4 == 1) && (estado_actual_s4 == 0) && (bloqueo_s4_ms == 0)) {
       fin_partido = !fin_partido;
       if (!fin_partido) {
           estado_led = false;
           gpio_set_level(LED_ROJO, 0);
           acumulador_led_ms = 0;
       }
       bloqueo_s4_ms = 50;
   }
   // Guardar estados
   estado_anterior_s1 = estado_actual_s1;
   estado_anterior_s2 = estado_actual_s2;
   estado_anterior_s3 = estado_actual_s3;
   estado_anterior_s4 = estado_actual_s4;
   // Parpadeo LED en fin de partido
   if (fin_partido) {
       acumulador_led_ms += 5;
       // 2 Hz -> cambia cada 250 ms
       if (acumulador_led_ms >= 250) {
           acumulador_led_ms = 0;
           estado_led = !estado_led;
           gpio_set_level(LED_ROJO, estado_led);
       }
   }
   return false;
}

// ==================== MAIN ====================
void app_main() {
   // Configurar segmentos como salida
   gpio_config_t out_seg = {
       .pin_bit_mask =
           (1ULL << SEG_A) |
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
   gpio_config(&out_seg);
   // Configurar dígitos como salida
   gpio_config_t out_dig = {
       .pin_bit_mask =
           (1ULL << DIG_1) |
           (1ULL << DIG_2) |
           (1ULL << DIG_3) |
           (1ULL << DIG_4),
       .mode = GPIO_MODE_OUTPUT,
       .pull_up_en = GPIO_PULLUP_DISABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&out_dig);
   // Configurar LED como salida
   gpio_config_t out_led = {
       .pin_bit_mask = (1ULL << LED_ROJO),
       .mode = GPIO_MODE_OUTPUT,
       .pull_up_en = GPIO_PULLUP_DISABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&out_led);
   // Configurar botones como entrada
   gpio_config_t in_cfg = {
       .pin_bit_mask =
           (1ULL << BTN_S1) |
           (1ULL << BTN_S2) |
           (1ULL << BTN_S3) |
           (1ULL << BTN_S4),
       .mode = GPIO_MODE_INPUT,
       .pull_up_en = GPIO_PULLUP_ENABLE,
       .pull_down_en = GPIO_PULLDOWN_DISABLE,
       .intr_type = GPIO_INTR_DISABLE
   };
   gpio_config(&in_cfg);
   // Estado inicial
   apagar_digitos();
   cargar_segmentos(0);
   gpio_set_level(LED_ROJO, 0);
   // Configuración del timer
   timer_config_t timer_cfg = {
       .divider = TIMER_DIVIDER,
       .counter_dir = TIMER_COUNT_UP,
       .counter_en = TIMER_PAUSE,
       .alarm_en = TIMER_ALARM_EN,
       .auto_reload = true
   };
   timer_init(TIMER_GROUP_0, TIMER_0, &timer_cfg);
   timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
   timer_set_alarm_value(
       TIMER_GROUP_0,
       TIMER_0,
       TIMER_INTERVAL_US
   );
   timer_isr_callback_add(
       TIMER_GROUP_0,
       TIMER_0,
       timer_isr,
       NULL,
       0
   );
   timer_enable_intr(TIMER_GROUP_0, TIMER_0);
   timer_start(TIMER_GROUP_0, TIMER_0);
   while (1) {
       vTaskDelay(pdMS_TO_TICKS(1));
   }
} 
*/