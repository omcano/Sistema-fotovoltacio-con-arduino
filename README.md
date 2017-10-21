# Sistema-fotovoltacio-con-arduino
Sistema fotovoltaico con seguimiento del punto de máxima potenica (MPPT) controlado con Arduino. Regulador de tensión de entrada tipo Boost, 
inversor puente en H con NMOSFET y GUI de MatLab para monitorizar la potencia.
El convertidor tipo Boost es un tipo de fuente conmutada que nos permite variar la tensión sin perder potencia en el proceso. De esta manera, y
haciendo medidas de la tensión y de la corriente, podemos calcular la potencia en todo momento.
El inversor nos va a permitr generar una señal de alterna de 50Hz mediente la comutación de los transistores con señales PWM generadas con Arduino de la manera que se explica en la documentación.
Por último, un filtro LC eliminará las componentes de alta frecuancia y nos dejará un señal de 50Hz para uso doméstico.

