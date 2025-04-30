/*

*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <time.h>
#include "esp_system.h"
#include "esp_log.h"
#include <sys/time.h>
#include "ili9341.h"
#include "digitos.h"

#define DIGITO_ANCHO 40
#define DIGITO_ALTO 80
#define DIGITO_ENCENDIDO ILI9341_RED
#define DIGITO_APAGADO 0x3800
#define DIGITO_FONDO ILI9341_BLACK

static const char *TAG = "RTC_Example";

void obtenerFechaHora(void *p)
{
    time_t t;
    struct tm timeinfo;
    while (1)
    {
        time(&t);
        localtime_r(&t, &timeinfo);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void actualizarPantalla(void *p)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    ILI9341Init();
    ILI9341Rotate(ILI9341_Landscape_1);

    panel_t PanelHoras = CrearPanel(9, 0, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelMinutos = CrearPanel(113, 0, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelSegundos = CrearPanel(223, 0, 2, DIGITO_ALTO, DIGITO_ANCHO, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelDia = CrearPanel(40, 120, 2, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelMes = CrearPanel(110, 120, 2, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);
    panel_t PanelAnio = CrearPanel(180, 120, 4, DIGITO_ALTO - 40, DIGITO_ANCHO - 40, DIGITO_ENCENDIDO, DIGITO_APAGADO, DIGITO_FONDO);

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
    ILI9341DrawString(95, 130, "-", &font_11x18, DIGITO_ENCENDIDO, DIGITO_FONDO);
    ILI9341DrawString(165, 130, "-", &font_11x18, DIGITO_ENCENDIDO, DIGITO_FONDO);

    int year = timeinfo.tm_year + 1900;
    DibujarDigito(PanelAnio, 0, (year / 1000) % 10);
    DibujarDigito(PanelAnio, 1, (year / 100) % 10);
    DibujarDigito(PanelAnio, 2, (year / 10) % 10);
    DibujarDigito(PanelAnio, 3, year % 10);

    struct tm timeinfo_prev = timeinfo;
    int year_prev = year;

    while (1)
    {
        time(&now);
        localtime_r(&now, &timeinfo);

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
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // Configurar el RTC con una hora inicial (por ejemplo, 1 de enero de 2025, 12:00:00)
    struct tm timeinfo = {
        .tm_year = 2025 - 1900, // Año desde 1900
        .tm_mon = 0,            // Mes (0 = enero)
        .tm_mday = 1,           // Día del mes
        .tm_hour = 12,          // Hora
        .tm_min = 0,            // Minuto
        .tm_sec = 0             // Segundo
    };

    time_t t = mktime(&timeinfo);
    struct timeval now = {.tv_sec = t};
    settimeofday(&now, NULL);

    xTaskCreate(obtenerFechaHora, "ImprimirFechaHora", 2048, NULL, 1, NULL);
    xTaskCreate(actualizarPantalla, "ActualizarPantalla", 2048, NULL, 1, NULL);
}
