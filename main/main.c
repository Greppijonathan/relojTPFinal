#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <time.h>
#include "esp_system.h"
#include "esp_log.h"
#include <sys/time.h>
#include "ili9341.h"
#include "digitos.h"
#include "teclasconfig.h"
#include "leds.h"

#define DIGITO_ANCHO 40
#define DIGITO_ALTO 80
#define DIGITO_ENCENDIDO ILI9341_RED
// #define DIGITO_AJUSTE ILI9341_BLUE2
#define DIGITO_APAGADO 0x3800
#define DIGITO_FONDO ILI9341_BLACK

bool modoAjusteReloj = false;
bool alarmaActivada = false;
static const char *TAG = "RTC";

QueueHandle_t colaControlTiempo;

#define EVENTO_100MS (1 << 0)
#define EVENTO_1S (1 << 1)

// Variables globales para la alarma
static int alarmaHoras = 12;
static int alarmaMinutos = 1;
bool modoAjusteAlarma = false;
int modo = -1;

void verificarAlarma(void *p)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    time_t now;
    struct tm timeinfo;

    while (true)
    {
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_hour == alarmaHoras && timeinfo.tm_min == alarmaMinutos && timeinfo.tm_sec == 0)
        {
            ESP_LOGI("ALARMA", "❗ ALARMA ACTIVADA! Son las %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            alarmaActivada = true;
            PrenderLedAzul(true);
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}

void ajustarReloj(void *p)
{
    configuracion_pin_t configuraciones[] = {
        {TEC1_Config, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY},
        {TEC2_Inc, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY},
        {TEC3_Dec, GPIO_MODE_INPUT, GPIO_PULLUP_ONLY}};

    ConfigurarTeclas(configuraciones, sizeof(configuraciones) / sizeof(configuraciones[0]));
    ConfigurarSalidasLed();
    apagarLeds();
    struct tm timeinfo;
    time_t t;
    bool modoAjusteReloj = false;
    TickType_t tiempoInicioPresionado = 0;

    while (1)
    {
        if (!gpio_get_level(TEC1_Config))
        {
            if (tiempoInicioPresionado == 0)
            {
                tiempoInicioPresionado = xTaskGetTickCount();
            }
            else if ((xTaskGetTickCount() - tiempoInicioPresionado) >= pdMS_TO_TICKS(2000))
            {
                modoAjusteReloj = false;
                modoAjusteAlarma = false;
                ESP_LOGI("CONFIG", "✅ Alarma configurada! en %02d:%02d", alarmaHoras, alarmaMinutos);
                tiempoInicioPresionado = 0;
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        else
        {
            if (tiempoInicioPresionado != 0)
            {
                if (alarmaActivada)
                {
                    alarmaActivada = false;
                    ESP_LOGI("CONFIG", "❌ Alarma desactivada por teclado!");
                    apagarLeds();
                }
                modo = (modo + 1) % 7;
                modoAjusteReloj = (modo < 5);
                modoAjusteAlarma = (modo >= 5);

                ESP_LOGI("AJUSTE", "Se ajusta: %s",
                         (modo == 0) ? "Minutos" : (modo == 1) ? "Horas"
                                               : (modo == 2)   ? "Día"
                                               : (modo == 3)   ? "Mes"
                                               : (modo == 4)   ? "Año"
                                               : (modo == 5)   ? "Minutos de la Alarma"
                                                               : "Horas de la Alarma");

                tiempoInicioPresionado = 0;
                vTaskDelay(pdMS_TO_TICKS(300));
            }
        }

        if (modoAjusteReloj || modoAjusteAlarma)
        {
            if (!gpio_get_level(TEC2_Inc))
            {
                time(&t);
                localtime_r(&t, &timeinfo);

                if (modoAjusteReloj)
                {
                    switch (modo)
                    {
                    case 0:
                        timeinfo.tm_min = (timeinfo.tm_min + 1) % 60;
                        break;
                    case 1:
                        timeinfo.tm_hour = (timeinfo.tm_hour + 1) % 24;
                        break;
                    case 2:
                        timeinfo.tm_mday = (timeinfo.tm_mday % 31) + 1;
                        break;
                    case 3:
                        timeinfo.tm_mon = (timeinfo.tm_mon + 1) % 12;
                        break;
                    case 4:
                        timeinfo.tm_year = timeinfo.tm_year + 1;
                        break;
                    }
                    t = mktime(&timeinfo);
                    struct timeval now = {.tv_sec = t};
                    settimeofday(&now, NULL);

                    ESP_LOGI("CONFIG", "Hora %02d, Minuto %02d, fecha %02d/%02d/%04d, alarma %02d:%02d",
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_mday, timeinfo.tm_mon + 1,
                             timeinfo.tm_year + 1900, alarmaHoras, alarmaMinutos);
                }
                else
                {
                    if (modo == 5)
                        alarmaMinutos = (alarmaMinutos + 1) % 60;
                    if (modo == 6)
                        alarmaHoras = (alarmaHoras + 1) % 24;

                    ESP_LOGI("CONFIG", "Hora %02d, Minuto %02d, fecha %02d/%02d/%04d, alarma %02d:%02d",
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_mday, timeinfo.tm_mon + 1,
                             timeinfo.tm_year + 1900, alarmaHoras, alarmaMinutos);
                }

                vTaskDelay(pdMS_TO_TICKS(300));
            }

            if (!gpio_get_level(TEC3_Dec))
            {
                time(&t);
                localtime_r(&t, &timeinfo);

                if (modoAjusteReloj)
                {
                    switch (modo)
                    {
                    case 0:
                        timeinfo.tm_min = (timeinfo.tm_min == 0 ? 59 : timeinfo.tm_min - 1);
                        break;
                    case 1:
                        timeinfo.tm_hour = (timeinfo.tm_hour == 0 ? 23 : timeinfo.tm_hour - 1);
                        break;
                    case 2:
                        timeinfo.tm_mday = (timeinfo.tm_mday == 1 ? 31 : timeinfo.tm_mday - 1);
                        break;
                    case 3:
                        timeinfo.tm_mon = (timeinfo.tm_mon == 0 ? 11 : timeinfo.tm_mon - 1);
                        break;
                    case 4:
                        timeinfo.tm_year = timeinfo.tm_year - 1;
                        break;
                    }
                    t = mktime(&timeinfo);
                    struct timeval now = {.tv_sec = t};
                    settimeofday(&now, NULL);

                    ESP_LOGI("CONFIG", "Hora %02d, Minuto %02d, fecha %02d/%02d/%04d, alarma %02d:%02d",
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_mday, timeinfo.tm_mon + 1,
                             timeinfo.tm_year + 1900, alarmaHoras, alarmaMinutos);
                }
                else
                {
                    if (modo == 5)
                        alarmaMinutos = (alarmaMinutos == 0 ? 59 : alarmaMinutos - 1);
                    if (modo == 6)
                        alarmaHoras = (alarmaHoras == 0 ? 23 : alarmaHoras - 1);

                    ESP_LOGI("CONFIG", "Hora %02d, Minuto %02d, fecha %02d/%02d/%04d, alarma %02d:%02d",
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_mday, timeinfo.tm_mon + 1,
                             timeinfo.tm_year + 1900, alarmaHoras, alarmaMinutos);
                }

                vTaskDelay(pdMS_TO_TICKS(300));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void controlTiempo(void *p)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t eventos;

    while (1)
    {
        eventos = EVENTO_100MS;
        xQueueSend(colaControlTiempo, &eventos, 0);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));

        if ((xLastWakeTime % pdMS_TO_TICKS(1000)) == 0)
        {
            eventos = EVENTO_1S;
            xQueueSend(colaControlTiempo, &eventos, 0);
        }
    }
}

void obtenerFechaHora(void *p)
{
    time_t t;
    struct tm timeinfo;
    uint8_t evento;

    while (1)
    {

        if (xQueueReceive(colaControlTiempo, &evento, portMAX_DELAY))
        {
            if (evento & EVENTO_100MS)
            {
                time(&t);
                localtime_r(&t, &timeinfo);
            }
        }
    }
}
void actualizarPantalla(void *p)
{
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    panel_t PanelHoras = CrearPanel(9, 0, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelMinutos = CrearPanel(113, 0, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelSegundos = CrearPanel(223, 0, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelDia = CrearPanel(40, 100, 2, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelMes = CrearPanel(110, 100, 2, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelAnio = CrearPanel(180, 100, 4, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelHorasAlarma = CrearPanel(110, 165, 2, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelMinutosAlarma = CrearPanel(170, 165, 2, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);

    DibujarDigito(PanelHoras, 0, timeinfo.tm_hour / 10);
    DibujarDigito(PanelHoras, 1, timeinfo.tm_hour % 10);
    DibujarDigito(PanelMinutos, 0, timeinfo.tm_min / 10);
    DibujarDigito(PanelMinutos, 1, timeinfo.tm_min % 10);
    DibujarDigito(PanelSegundos, 0, timeinfo.tm_sec / 10);
    DibujarDigito(PanelSegundos, 1, timeinfo.tm_sec % 10);
    DibujarDigito(PanelDia, 0, timeinfo.tm_mday / 10);
    DibujarDigito(PanelDia, 1, timeinfo.tm_mday % 10);
    DibujarDigito(PanelMes, 0, (timeinfo.tm_mon + 1) / 10);
    DibujarDigito(PanelMes, 1, (timeinfo.tm_mon + 1) % 10);

    ILI9341DrawCircle(100, 25, 3, DIGITO_ENCENDIDO);
    ILI9341DrawCircle(100, 55, 3, DIGITO_ENCENDIDO);
    ILI9341DrawCircle(210, 25, 3, DIGITO_ENCENDIDO);
    ILI9341DrawCircle(210, 55, 3, DIGITO_ENCENDIDO);
    ILI9341DrawString(95, 110, "-", &font_11x18, DIGITO_ENCENDIDO, DIGITO_FONDO);
    ILI9341DrawString(165, 110, "-", &font_11x18, DIGITO_ENCENDIDO, DIGITO_FONDO);

    int year = timeinfo.tm_year + 1900;
    DibujarDigito(PanelAnio, 0, (year / 1000) % 10);
    DibujarDigito(PanelAnio, 1, (year / 100) % 10);
    DibujarDigito(PanelAnio, 2, (year / 10) % 10);
    DibujarDigito(PanelAnio, 3, year % 10);
    DibujarDigito(PanelHorasAlarma, 0, 0);
    DibujarDigito(PanelHorasAlarma, 1, 0);

    DibujarDigito(PanelHorasAlarma, 0, (alarmaHoras / 10) % 10);
    DibujarDigito(PanelHorasAlarma, 1, alarmaHoras % 10);

    DibujarDigito(PanelMinutosAlarma, 0, (alarmaMinutos / 10) % 10);
    DibujarDigito(PanelMinutosAlarma, 1, alarmaMinutos % 10);

    struct tm timeinfo_prev = timeinfo;
    int year_prev = year;
    uint8_t evento;
    int horasAlarmaAnterior = alarmaHoras;
    int minutosAlarmaAnterior = alarmaMinutos;

    while (1)
    {
        if (xQueueReceive(colaControlTiempo, &evento, portMAX_DELAY))
        {
            if (evento & EVENTO_100MS)
            {
                time(&now);
                localtime_r(&now, &timeinfo);

                if (alarmaHoras != horasAlarmaAnterior)
                {
                    DibujarDigito(PanelHorasAlarma, 0, (alarmaHoras / 10) % 10);
                    DibujarDigito(PanelHorasAlarma, 1, alarmaHoras % 10);
                    horasAlarmaAnterior = alarmaHoras;
                }
                if (alarmaMinutos != minutosAlarmaAnterior)
                {
                    DibujarDigito(PanelMinutosAlarma, 0, (alarmaMinutos / 10) % 10);
                    DibujarDigito(PanelMinutosAlarma, 1, alarmaMinutos % 10);
                    minutosAlarmaAnterior = alarmaMinutos;
                }
                if (timeinfo.tm_hour != timeinfo_prev.tm_hour)
                {
                    DibujarDigito(PanelHoras, 0, timeinfo.tm_hour / 10);
                    DibujarDigito(PanelHoras, 1, timeinfo.tm_hour % 10);
                }
                if (timeinfo.tm_min != timeinfo_prev.tm_min)
                {
                    DibujarDigito(PanelMinutos, 0, timeinfo.tm_min / 10);
                    DibujarDigito(PanelMinutos, 1, timeinfo.tm_min % 10);
                }
                if (timeinfo.tm_sec != timeinfo_prev.tm_sec)
                {
                    DibujarDigito(PanelSegundos, 0, timeinfo.tm_sec / 10);
                    DibujarDigito(PanelSegundos, 1, timeinfo.tm_sec % 10);
                }
                if (timeinfo.tm_mday != timeinfo_prev.tm_mday)
                {
                    DibujarDigito(PanelDia, 0, timeinfo.tm_mday / 10);
                    DibujarDigito(PanelDia, 1, timeinfo.tm_mday % 10);
                }
                if (timeinfo.tm_mon != timeinfo_prev.tm_mon)
                {
                    DibujarDigito(PanelMes, 0, (timeinfo.tm_mon + 1) / 10);
                    DibujarDigito(PanelMes, 1, (timeinfo.tm_mon + 1) % 10);
                }
                year = timeinfo.tm_year + 1900;
                if (year != year_prev)
                {
                    DibujarDigito(PanelAnio, 0, (year / 1000) % 10);
                    DibujarDigito(PanelAnio, 1, (year / 100) % 10);
                    DibujarDigito(PanelAnio, 2, (year / 10) % 10);
                    DibujarDigito(PanelAnio, 3, year % 10);
                    year_prev = year;
                }
                timeinfo_prev = timeinfo;
            }
        }
    }
}

void app_main(void)
{
    struct tm timeinfo = {
        .tm_year = 2025 - 1900,
        .tm_mon = 0,
        .tm_mday = 1,
        .tm_hour = 12,
        .tm_min = 0,
        .tm_sec = 0};

    time_t t = mktime(&timeinfo);
    struct timeval now = {.tv_sec = t};
    settimeofday(&now, NULL);

    colaControlTiempo = xQueueCreate(10, sizeof(uint8_t));

    xTaskCreate(obtenerFechaHora, "ObtieneFechaHora", 2048, NULL, 1, NULL);
    xTaskCreate(controlTiempo, "TiempoParaTareas", 2048, NULL, 2, NULL);
    xTaskCreate(actualizarPantalla, "ActualizarPantalla", 2048, NULL, 1, NULL);
    xTaskCreate(ajustarReloj, "ConfigurarFechaHora", 2048, NULL, 1, NULL);
    xTaskCreate(verificarAlarma, "VerificarAlarma", 2048, NULL, 1, NULL);
}
