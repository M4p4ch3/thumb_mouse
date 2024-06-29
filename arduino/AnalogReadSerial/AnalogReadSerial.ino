
//* Conf

#define PIN_LEFT_VERTICAL       PIN_A0
#define PIN_LEFT_HORIZONTAL     PIN_A1
#define PIN_RIGHT_VERTICAL      PIN_A2
#define PIN_RIGHT_HORIZONTAL    PIN_A3

#define VAL_MAX 677
#define VAL_MIN 0
#define VAL_DEADZONE 20

#define SERIAL_TX_DELAY_MS 10U

// Conf */

#define VAL_NB 4U
#define SERIAL_MSG_VAL_DFLT "000,"
#define SERIAL_MSG_VAL_SIZE sizeof(SERIAL_MSG_VAL_DFLT)
#define SERIAL_MSG_DFLT "000,000,000,000,"
#define SERIAL_MSG_SIZE sizeof(SERIAL_MSG_DFLT)

uint8_t PIN_ARRAY[VAL_NB] = {
    PIN_LEFT_VERTICAL,
    PIN_LEFT_HORIZONTAL,
    PIN_RIGHT_VERTICAL,
    PIN_RIGHT_HORIZONTAL,
};

const int valMid = int((VAL_MAX - VAL_MIN) / 2);
int g_pVal[VAL_NB] = {0};
char g_sSerialMsg[SERIAL_MSG_SIZE] = SERIAL_MSG_DFLT;

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    bool bSend = false;
    uint8_t valIdx = 0U;

    for (valIdx = 0U; valIdx < VAL_NB; valIdx += 1U)
    {
        g_pVal[valIdx] = analogRead(PIN_ARRAY[valIdx]);

        snprintf(&g_sSerialMsg[(SERIAL_MSG_VAL_SIZE - 1U) * valIdx], SERIAL_MSG_VAL_SIZE, "%03d,", g_pVal[valIdx]);

        if (int(abs(g_pVal[valIdx] - valMid) / 2U) > VAL_DEADZONE)
        {
            bSend = true;
        }
    }

    if (bSend)
    {
        Serial.println(&g_sSerialMsg[0U]);
    }

    delay(SERIAL_TX_DELAY_MS);
}
