# UpdaterDLL
Updater avanzado en C/C++ para cualquier tipo de juego/software/app de windows.

Liberado oficialmente en:
nosetu.com

🔄 Nuevo Updater en C++: Automatiza Actualizaciones de Software
Hoy estoy liberando un Updater en C++, diseñado para facilitar la gestión de actualizaciones en cualquier cliente, software, juego o aplicación. Este proyecto se centra en automatizar la descarga, verificación y aplicación de actualizaciones de manera eficiente y con un enfoque modular para adaptarse a múltiples escenarios.



🚀 Características principales:
Verificación de versiones: Comprueba versiones locales y remotas antes de proceder con las actualizaciones.
Descarga de archivos remotos: Utiliza WinHTTP para gestionar la comunicación con servidores HTTP/HTTPS.
Extracción de ZIPs: Emplea la API de Shell de Windows para descomprimir archivos de actualización.
Logs y manejo de errores: Registra errores y progreso, mostrando mensajes claros tanto en consola como en archivos de log.
Copia de seguridad automática: Preserva versiones anteriores antes de aplicar nuevas actualizaciones.

🛠️ ¿Cómo funciona?
Detección de versiones:

Lee las versiones locales desde archivos en el sistema.
Compara con las versiones remotas obtenidas a través de solicitudes HTTP.
Descarga de actualizaciones:

Si se detectan diferencias, el Updater descarga los archivos necesarios (ejecutables o recursos) desde URLs configuradas.
Aplicación de actualizaciones:

Extrae los archivos descargados en directorios específicos.
Reemplaza ejecutables y otros recursos de forma segura, generando una copia de seguridad automática.
Finalización:

Muestra mensajes indicando el éxito o cualquier error encontrado durante el proceso.
Opcionalmente, reinicia el cliente/software actualizado.

📚 APIs y funciones utilizadas:
WinHTTP: Para manejar solicitudes HTTP/HTTPS.
Shell API de Windows: Para extraer archivos comprimidos en formato ZIP.
Funciones de consola: Para mostrar información en tiempo real sobre el progreso de la actualización.
Gestión de errores con GetLastError: Para diagnosticar problemas de sistema durante la ejecución.

📦 Guía básica para usarlo:
Configura el entorno:
Ajusta las URLs en el código fuente para que apunten a los archivos de actualización de tu aplicación.
Compila el proyecto:
Utiliza cualquier IDE compatible con C++ (como Visual Studio) para compilar el código.
Integra con tu aplicación:
Llama a la función exportada CheckUpdates y pasa como parámetro la ruta base de tu aplicación.

🤝 Contribuciones bienvenidas:
Este Updater es un proyecto de código abierto y está pensado para que la comunidad lo adopte y lo mejore. Si tienes ideas, optimizaciones o mejoras, ¡no dudes en contribuir!



🔗 Repositorios en GitHub:
https://github.com/KevinKorduner?tab=repositories
