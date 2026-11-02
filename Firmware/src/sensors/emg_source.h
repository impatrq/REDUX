// emg_source.h · Fuente de la señal EMG elegida POR CANAL (bíceps y tríceps por
// separado): cada canal es real por ADC o simulado por serie. Guarda la fuente de cada
// canal y, para los simulados, la intensidad fijada por comando ("b N"/"t N"/"r").
// Lógica pura: no toca el ADC ni la interfaz; el llamador (app) lee el hardware de los
// canales ADC y emite los mensajes de la ui. Se prueba en PC. Depende solo de stdbool.
#ifndef REDUX_EMG_SOURCE_H
#define REDUX_EMG_SOURCE_H

#include <stdbool.h>

// Fuente de un canal.
typedef enum {
    EMG_SRC_ADC,    // señal real por ADC
    EMG_SRC_SERIAL, // señal simulada por comandos de serie
} emg_src_t;

// Resultado de alimentar una línea de comando (el llamador lo mapea a mensajes de la ui).
typedef enum {
    EMG_CMD_NONE,       // no es "b N"/"t N"/"r": que la procese otro (la interfaz de usuario)
    EMG_CMD_OK,         // "b N"/"t N" aplicado a un canal serial, o "r"
    EMG_CMD_BIC_IS_ADC, // "b N" pero el bíceps es ADC: comando ignorado (avisar)
    EMG_CMD_TRI_IS_ADC, // "t N" pero el tríceps es ADC: comando ignorado (avisar)
} emg_cmd_result_t;

// Estado de la fuente. El llamador declara uno y lo pasa por puntero; sin globales ocultas.
typedef struct {
    emg_src_t bic_src;
    emg_src_t tri_src;
    int bic_pct; // intensidad simulada del bíceps [0,100] (solo si bic_src == EMG_SRC_SERIAL)
    int tri_pct; // intensidad simulada del tríceps [0,100] (solo si tri_src == EMG_SRC_SERIAL)
} emg_source_t;

// Inicializa con la fuente de cada canal. Deja los canales simulados en reposo (0 %).
void emg_source_init(emg_source_t *s, emg_src_t bic_src, emg_src_t tri_src);

// true si el canal es simulado (fuente SERIAL): su calibración se autocompleta y se
// informa como simulado (simB/simT en el reporte).
bool emg_source_bic_is_sim(const emg_source_t *s);
bool emg_source_tri_is_sim(const emg_source_t *s);

// Intensidad simulada del canal, acotada a [0,1] (0 si el canal es ADC).
float emg_source_bic_level(const emg_source_t *s);
float emg_source_tri_level(const emg_source_t *s);

// Procesa una línea de comando de la fuente simulada:
//   "b N" -> bíceps al N %   "t N" -> tríceps al N %   "r" -> reposo de los simulados.
// "b N"/"t N" solo se aplican si ese canal es SERIAL; si es ADC devuelve EMG_CMD_*_IS_ADC
// sin tocar nada (el llamador avisa). "r" pone a 0 solo los canales simulados.
emg_cmd_result_t emg_source_feed(emg_source_t *s, const char *line);

#endif // REDUX_EMG_SOURCE_H
