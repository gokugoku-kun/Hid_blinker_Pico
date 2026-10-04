from pico_led import PicoLed

def connect_pico():
    try:
        return PicoLed()
    except RuntimeError as exc:
        print(f"[INFO] Pico W未接続: {exc}")
        return None


pico = connect_pico()

if pico is not None:
    pico.all_off()

if pico is not None:
    pico.blink(0, 250, 250)
    pico.blink(1, 250, 250)
