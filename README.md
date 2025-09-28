# TPE-Arqui

## Que es?

Es un kernelcito que puede correr una shell basica y un jueguito de golf muy divertido.

## Compartirlo online la VM de quemu

### Parsec (Opción más recomendada y efectiva)
Parsec es excelente para compartir escritorios completos con baja latencia, lo cual es ideal para tu escenario. Aunque está pensado para juegos, puede compartir cualquier cosa que se muestre en tu pantalla.

**Cómo funciona**: Instalarías Parsec en tu sistema operativo anfitrión (donde tienes QEMU corriendo). Cuando inicies la máquina virtual de QEMU y tu kernel esté corriendo, simplemente compartirías tu pantalla completa (o la ventana de QEMU si Parsec lo permite específicamente). Tu amigo vería tu escritorio y la ventana de QEMU, y sus entradas de teclado se dirigirían a tu computadora, donde Parsec las enviaría a QEMU.
Ventajas:
Baja latencia: Crucial para la interactividad del juego y para sentir que tu amigo está "realmente" controlando.
Calidad de imagen: Muy buena, permitiendo apreciar los detalles de tu kernel y juego.
Compatibilidad con teclados: Parsec está diseñado para pasar las entradas de teclado y ratón de forma eficiente. Tu amigo podría usar sus "flechitas" para controlar el juego dentro de la máquina virtual.
Pasos generales:
Asegúrate de que QEMU esté configurado para mostrar la interfaz gráfica de tu kernel en una ventana en tu escritorio.
Descarga e instala Parsec en tu computadora.
Crea una cuenta en Parsec e inicia sesión.
Inicia la máquina virtual con QEMU y tu kernel/juego.
En la aplicación de Parsec, configura tu computadora como host y genera un enlace o código de invitación.
Tu amigo descarga e instala Parsec, inicia sesión y usa el enlace/código para conectarse a tu máquina.
Una vez conectado, Parsec te preguntará qué pantalla quieres compartir. Elige la pantalla donde está la ventana de QEMU.

### AnyDesk o TeamViewer (Alternativa si Parsec no funciona)
Estas son herramientas de escritorio remoto más generales. También te permiten compartir tu pantalla completa, incluyendo la ventana de QEMU.

**Cómo funciona**: Al igual que con Parsec, las instalarías en tu sistema anfitrión. Cuando tu amigo se conecte, verá tu escritorio y la ventana de QEMU. Sus pulsaciones de teclado se enviarán a tu computadora.
Ventajas:
Fáciles de usar: La configuración es bastante sencilla.
Desventajas:
Mayor latencia: La experiencia de juego podría no ser tan fluida como con Parsec, lo que es un factor importante para un juego.
Menor calidad de imagen: La compresión puede ser más agresiva, afectando la nitidez.
Pasos generales:
Asegúrate de que QEMU esté mostrando tu kernel en una ventana.
Descarga e instala AnyDesk o TeamViewer en ambas computadoras.
En tu computadora, verás un ID y contraseña. Compártelos con tu amigo.
Tu amigo ingresará tu ID en su aplicación y luego la contraseña para establecer la conexión.
Una vez conectado, tu amigo verá tu escritorio y podrá interactuar con él, incluyendo la ventana de QEMU.
¿Por qué Steam Remote Play Together no funciona bien aquí?
Steam Remote Play Together busca un "juego" o un ejecutable específico que esté en tu biblioteca de Steam para engancharse a él y transmitir solo ese proceso. Dado que tu "juego" es en realidad tu kernel corriendo dentro de QEMU, Steam no puede "ver" el juego como una aplicación independiente. Estaría intentando transmitir el proceso de QEMU, que no es el juego en sí, y la entrada de teclado podría no mapearse correctamente al entorno virtualizado.






