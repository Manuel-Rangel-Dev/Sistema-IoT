import re
import sys

patron = re.compile(r'^-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,\d+')

archivo_entrada = sys.argv[1]
archivo_salida  = archivo_entrada.replace(".log", ".csv")

with open(archivo_entrada, "r") as f_in, open(archivo_salida, "w") as f_out:
    f_out.write("rpm,current_A,voltage_V,power_W,pwm\n")
    for linea in f_in:
        linea = linea.strip()
        if patron.match(linea):
            f_out.write(linea + "\n")

print(f"CSV guardado en: {archivo_salida}")