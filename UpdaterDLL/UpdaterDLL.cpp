#define UNICODE
#define _UNICODE

#include "UpdaterDLL.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <fcntl.h>
#include <io.h>
#include <shlobj.h>
#include <shlwapi.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

// Variable global para controlar el modo de depuración
bool ModoDebug = false; // Cambia a true para habilitar el modo de depuración

// Variable global para controlar si se muestran detalles en consola
bool MostrarDetallesConsola = false; // Cambia a false para mostrar menos detalles

// Variable global para acumular mensajes de error
std::string g_errorMessages;

// Función para mostrar mensajes en la consola
void ShowMessage(const std::string& message) {
    if (MostrarDetallesConsola) {
        std::cout << message << std::endl;
    }
}

// Función para registrar mensajes de error
void LogError(const std::string& errorMessage) {
    g_errorMessages += errorMessage + "\n";
    std::cout << errorMessage << std::endl; // Siempre mostrar errores
}

// Función para obtener el mensaje de error del sistema
std::string GetLastErrorAsString() {
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
        return std::string(); // No hay error

    LPSTR messageBuffer = nullptr;

    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    LocalFree(messageBuffer);

    return message;
}

// Función para leer la versión desde un archivo
std::string ReadVersionFromFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file) {
        LogError("Error al abrir el archivo: " + filePath);
        return "";
    }
    std::string version;
    std::getline(file, version);
    file.close();
    // Eliminar espacios en blanco
    version.erase(std::remove_if(version.begin(), version.end(), ::isspace), version.end());
    return version;
}

// Función para leer la versión desde una URL usando WinHTTP
std::string ReadVersionFromURL(const std::string& url)
{
    std::wstring wUrl(url.begin(), url.end());

    // Analizar la URL
    URL_COMPONENTSW urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256];
    wchar_t urlPath[1024];
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        LogError("Error al analizar la URL: " + url);
        return "";
    }

    // Crear sesión WinHTTP
    HINTERNET hSession = WinHttpOpen(L"Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) {
        LogError("Error al abrir la sesión WinHTTP.");
        return "";
    }

    // Conectarse al servidor
    HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
    if (!hConnect) {
        LogError("Error al conectarse al servidor.");
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Configurar banderas para manejar SSL/TLS
    DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        dwFlags);
    if (!hRequest) {
        LogError("Error al abrir la solicitud HTTP.");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Enviar solicitud
    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResults) {
        LogError("Error al enviar la solicitud HTTP.");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Recibir respuesta
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        LogError("Error al recibir la respuesta HTTP.");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Leer datos
    std::string version;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            LogError("Error al consultar datos disponibles.");
            break;
        }

        if (dwSize == 0)
            break;

        char* buffer = new char[dwSize + 1];
        ZeroMemory(buffer, dwSize + 1);

        if (!WinHttpReadData(hRequest, (LPVOID)buffer, dwSize, &dwDownloaded)) {
            LogError("Error al leer datos.");
            delete[] buffer;
            break;
        }

        version.append(buffer, dwDownloaded);
        delete[] buffer;

    } while (dwSize > 0);

    // Cerrar handles
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Eliminar caracteres de nueva línea y retorno de carro
    version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
    version.erase(std::remove(version.begin(), version.end(), '\r'), version.end());

    // Mostrar el contenido obtenido para depuración o mensaje genérico
    if (ModoDebug) {
        ShowMessage("Contenido obtenido desde la URL [" + url + "]: [" + version + "]");
    }

    // Eliminar espacios en blanco
    version.erase(std::remove_if(version.begin(), version.end(), ::isspace), version.end());

    return version;
}

// Función para crear una carpeta si no existe
bool CreateDirectoryIfNotExists(const std::string& path) {
    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        // La carpeta no existe, intentar crearla
        if (!CreateDirectoryA(path.c_str(), NULL)) {
            LogError("No se pudo crear la carpeta: " + path + " - " + GetLastErrorAsString());
            return false;
        }
    }
    else if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        LogError("La ruta existe pero no es una carpeta: " + path);
        return false;
    }
    return true;
}

// Función para extraer ZIP utilizando la API de Shell de Windows
bool ExtractZip(const std::string& zipPath, const std::string& extractPath) {
    // Crear la carpeta de destino si no existe
    if (!CreateDirectoryIfNotExists(extractPath)) {
        return false;
    }

    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LogError("Error al inicializar COM: " + GetLastErrorAsString());
        return false;
    }

    IShellDispatch* pIShellDispatch = NULL;
    Folder* pToFolder = NULL;
    Folder* pFromFolder = NULL;
    VARIANT vDir, vFile, vOpt, vItems;
    VariantInit(&vDir);
    VariantInit(&vFile);
    VariantInit(&vOpt);
    VariantInit(&vItems);

    hr = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void**)&pIShellDispatch);
    if (SUCCEEDED(hr)) {
        // Carpeta de destino
        vDir.vt = VT_BSTR;
        vDir.bstrVal = SysAllocString(std::wstring(extractPath.begin(), extractPath.end()).c_str());
        hr = pIShellDispatch->NameSpace(vDir, &pToFolder);
        if (SUCCEEDED(hr) && pToFolder != NULL) {
            // Archivo ZIP de origen
            vFile.vt = VT_BSTR;
            vFile.bstrVal = SysAllocString(std::wstring(zipPath.begin(), zipPath.end()).c_str());
            hr = pIShellDispatch->NameSpace(vFile, &pFromFolder);
            if (SUCCEEDED(hr) && pFromFolder != NULL) {
                // Obtener los elementos del ZIP
                FolderItems* pItems = NULL;
                hr = pFromFolder->Items(&pItems);
                if (SUCCEEDED(hr) && pItems != NULL) {
                    // Encapsular pItems en un VARIANT
                    vItems.vt = VT_DISPATCH;
                    vItems.pdispVal = pItems;

                    // Opciones para CopyHere
                    vOpt.vt = VT_I4;
                    vOpt.lVal = FOF_NO_UI;

                    // Copiar los elementos a la carpeta de destino
                    hr = pToFolder->CopyHere(vItems, vOpt);

                    if (SUCCEEDED(hr)) {
                        // Esperar a que la extracción se complete
                        const int maxWaitTimeMs = 60000; // 60 segundos
                        const int checkIntervalMs = 500;  // 0.5 segundos
                        int waitedTime = 0;
                        bool extractionCompleted = false;

                        // Supongamos que estamos extrayendo el ejecutable
                        std::string extractedExePath = extractPath + "\\CLIENTE.exe";

                        while (waitedTime < maxWaitTimeMs) {
                            if (GetFileAttributesA(extractedExePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                extractionCompleted = true;
                                break;
                            }
                            Sleep(checkIntervalMs);
                            waitedTime += checkIntervalMs;
                        }

                        if (!extractionCompleted) {
                            LogError("Tiempo de espera agotado para la extraccion del ZIP: " + zipPath);
                            hr = E_FAIL;
                        }
                        else {
                            if (MostrarDetallesConsola) {
                                ShowMessage("Extraccion del parche completada exitosamente.");
                            }
                        }
                    }
                    else {
                        LogError("Error al copiar archivos del ZIP: " + GetLastErrorAsString());
                    }

                    // Liberar pItems
                    pItems->Release();
                }
                else {
                    LogError("No se pudieron obtener los elementos del ZIP: " + zipPath);
                }
                pFromFolder->Release();
            }
            else {
                LogError("No se pudo acceder al archivo ZIP: " + zipPath);
            }
            pToFolder->Release();
        }
        else {
            LogError("No se pudo acceder a la carpeta de destino: " + extractPath);
        }
        pIShellDispatch->Release();
    }
    else {
        LogError("No se pudo crear instancia de IShellDispatch.");
    }

    // Limpiar VARIANTs
    VariantClear(&vDir);
    VariantClear(&vFile);
    VariantClear(&vOpt);
    VariantClear(&vItems);

    CoUninitialize();

    return SUCCEEDED(hr);
}

// Función para descargar un archivo usando WinHTTP
bool DownloadFile(const std::string& url, const std::string& filePath, const std::string& downloadType) {
    std::wstring wUrl(url.begin(), url.end());

    // Analizar la URL
    URL_COMPONENTSW urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256];
    wchar_t urlPath[1024];
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        LogError("Error al analizar la URL: " + url);
        return false;
    }

    // Crear sesión WinHTTP
    HINTERNET hSession = WinHttpOpen(L"Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) {
        LogError("Error al abrir la sesión WinHTTP.");
        return false;
    }

    // Conectarse al servidor
    HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
    if (!hConnect) {
        LogError("Error al conectarse al servidor.");
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Configurar banderas para manejar SSL/TLS
    DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        dwFlags);
    if (!hRequest) {
        LogError("Error al abrir la solicitud HTTP.");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Enviar solicitud
    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResults) {
        LogError("Error al enviar la solicitud HTTP.");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Recibir respuesta
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        LogError("Error al recibir la respuesta HTTP.");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Obtener el tamaño total del archivo de la respuesta
    DWORD contentLength = 0;
    DWORD size = sizeof(contentLength);
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, NULL, &contentLength, &size, NULL)) {
        LogError("Error al obtener el tamaño del contenido.");
        contentLength = 0; // Si no se puede obtener, asumimos que no sabemos el tamaño
    }

    // Abrir archivo para escribir
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LogError("Error al crear el archivo: " + filePath + " - " + GetLastErrorAsString());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Leer datos y escribir en el archivo
    DWORD dwDownloaded = 0;
    BYTE buffer[8192];
    DWORD totalDownloaded = 0;

    if (MostrarDetallesConsola) {
        std::cout << "Descargando: ";
    }

    while (true) {
        DWORD dwAvailableSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwAvailableSize)) {
            LogError("Error al consultar datos disponibles.");
            break;
        }

        if (dwAvailableSize == 0)
            break;

        DWORD dwChunkSize = dwAvailableSize > sizeof(buffer) ? sizeof(buffer) : dwAvailableSize;

        if (!WinHttpReadData(hRequest, buffer, dwChunkSize, &dwDownloaded)) {
            LogError("Error al leer datos.");
            break;
        }

        DWORD dwWritten = 0;
        if (!WriteFile(hFile, buffer, dwDownloaded, &dwWritten, NULL)) {
            LogError("Error al escribir en el archivo.");
            break;
        }

        totalDownloaded += dwWritten;

        // Calcular y mostrar el progreso
        if (contentLength > 0) {
            int percentage = static_cast<int>((static_cast<double>(totalDownloaded) / contentLength) * 100);

            if (MostrarDetallesConsola) {
                // Mostrar la barra de progreso
                int barWidth = 50; // Ancho de la barra de progreso
                int pos = barWidth * percentage / 100; // Posición de llenado de la barra
                std::cout << "\rDescargando: [";
                for (int i = 0; i < barWidth; ++i) {
                    if (i < pos) std::cout << "="; // Parte llena de la barra
                    else std::cout << " "; // Parte vacía de la barra
                }
                std::cout << "] " << percentage << "% completado"; // Mostrar porcentaje
                std::cout.flush(); // Asegurarse de que el contenido se imprima
            }
            else {
                // Salida simplificada
                std::cout << "\rActualizando Cliente - " << downloadType << " " << percentage << "% completado";
                std::cout.flush();
            }
        }
    }

    std::cout << std::endl; // Para nueva línea después de la descarga

    // Cerrar handles
    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return true;
}

// Función para actualizar el ejecutable
bool UpdateExecutable(const std::string& appPath, int remoteExeVersion) {
    // Definir URLs y rutas
    std::string urlExe = "https://kevinkorduner.com/Actualizador/Ejecutable" + std::to_string(remoteExeVersion) + ".zip";
    std::string zipPath = appPath + "\\Ejecutable.zip";
    std::string tempExtractPath = appPath + "\\temp_extraction";
    std::string exeName = "cliente.exe";
    std::string backupExeName = "old_client.exe";

    // Descargar el ejecutable ZIP
    if (MostrarDetallesConsola) {
        ShowMessage("Descargando la ultima version del cliente...");
    }
    if (!DownloadFile(urlExe, zipPath, "Ejecutable")) {
        LogError("Error al descargar el ejecutable.");
        return false;
    }
    if (MostrarDetallesConsola) {
        ShowMessage("Descarga completada.");
    }

    // Extraer el ZIP en la carpeta temporal
    if (MostrarDetallesConsola) {
        ShowMessage("Extrayendo el ejecutable...");
    }
    if (!ExtractZip(zipPath, tempExtractPath)) {
        LogError("Error al extraer el ejecutable.");
        return false;
    }
    if (MostrarDetallesConsola) {
        ShowMessage("Extraccion completada.");
    }

    // Verificar que el ejecutable extraído existe en la carpeta temporal
    std::string extractedExePath = tempExtractPath + "\\" + exeName;
    if (GetFileAttributesA(extractedExePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        LogError("El ejecutable extraído no se encuentra en la carpeta temporal: " + extractedExePath);
        return false;
    }

    // Reemplazar el ejecutable existente
    std::string exePath = appPath + "\\" + exeName;
    std::string backupExePath = appPath + "\\" + backupExeName;

    // Hacer copia de seguridad del ejecutable existente
    if (GetFileAttributesA(exePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!MoveFileExA(exePath.c_str(), backupExePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            LogError("No se pudo crear una copia de seguridad del ejecutable existente: " + GetLastErrorAsString());
            return false;
        }
        if (MostrarDetallesConsola) {
            ShowMessage("Copia de seguridad del ejecutable existente creada: " + backupExePath);
        }
    }

    // Mover el nuevo ejecutable desde la carpeta temporal a appPath
    if (!MoveFileExA(extractedExePath.c_str(), exePath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        LogError("No se pudo mover el nuevo ejecutable a la ubicación principal: " + GetLastErrorAsString());
        return false;
    }
    if (MostrarDetallesConsola) {
        ShowMessage("Nuevo ejecutable movido a la ubicación principal: " + exePath);
    }

    // Eliminar el ZIP descargado y la carpeta temporal
    DeleteFileA(zipPath.c_str());
    std::string removeCommand = "cmd /C rmdir /S /Q \"" + tempExtractPath + "\"";
    system(removeCommand.c_str());

    if (MostrarDetallesConsola) {
        ShowMessage("El ejecutable ha sido actualizado exitosamente.");
    }

    return true;
}

// Función para actualizar los recursos
bool UpdateResources(const std::string& appPath, int localVersion, int remoteVersion) {
    if (MostrarDetallesConsola) {
        ShowMessage("Actualizando recursos...");
    }
    for (int version = localVersion + 1; version <= remoteVersion; ++version) {
        std::string urlResource = "https://kevinkorduner.com/actualizador/Recurso" + std::to_string(version) + ".zip";
        std::string zipPath = appPath + "\\Recurso" + std::to_string(version) + ".zip";
        std::string extractPath = appPath;

        if (MostrarDetallesConsola) {
            ShowMessage("Descargando recursos version " + std::to_string(version) + "...");
        }
        if (!DownloadFile(urlResource, zipPath, "Recurso")) {
            LogError("Error al descargar recursos version " + std::to_string(version));
            return false;
        }

        if (MostrarDetallesConsola) {
            ShowMessage("Extrayendo recursos version " + std::to_string(version) + "...");
        }
        if (!ExtractZip(zipPath, extractPath)) {
            LogError("Error al extraer recursos version " + std::to_string(version));
            return false;
        }

        // Eliminar el ZIP descargado
        DeleteFileA(zipPath.c_str());

        // Actualizar el archivo de versión local
        std::string localResourceVersionFile = appPath + "\\INIT\\ActualizadorRecursos.txt";
        std::ofstream versionFile(localResourceVersionFile);
        if (versionFile) {
            versionFile << version;
            versionFile.close();
        }
        else {
            LogError("No se pudo actualizar el archivo de version local: " + localResourceVersionFile);
            return false;
        }

        if (MostrarDetallesConsola) {
            ShowMessage("Recursos version " + std::to_string(version) + " actualizados exitosamente.");
        }
    }
    if (MostrarDetallesConsola) {
        ShowMessage("Recursos actualizados.");
    }
    return true;
}

// Función que ejecuta el proceso de actualización
void UpdateProcess(const std::string& appPath)
{
    try {
        std::string initPath = appPath + "\\INIT";
        std::string localExeVersionFile = initPath + "\\Actualizador.txt";
        std::string localResourceVersionFile = initPath + "\\ActualizadorRecursos.txt";

        // Leer versiones locales
        std::string localExeVersionStr = ReadVersionFromFile(localExeVersionFile);
        std::string localResourceVersionStr = ReadVersionFromFile(localResourceVersionFile);

        // Verificar que las versiones locales no estén vacías
        if (localExeVersionStr.empty() || localResourceVersionStr.empty()) {
            LogError("Las versiones locales no se pudieron leer.");
            return;
        }

        int localExeVersion = std::stoi(localExeVersionStr);
        int localResourceVersion = std::stoi(localResourceVersionStr);

        // Leer versiones remotas
        std::string remoteExeVersionStr = ReadVersionFromURL("https://kevinkorduner.com/actualizador/Actualizador.txt");
        std::string remoteResourceVersionStr = ReadVersionFromURL("https://kevinkorduner.com/actualizador/ActualizadorRecursos.txt");

        // Verificar que las versiones remotas sean válidas
        if (remoteExeVersionStr.empty() || !std::all_of(remoteExeVersionStr.begin(), remoteExeVersionStr.end(), ::isdigit)) {
            remoteExeVersionStr = localExeVersionStr; // Evitar actualización
        }

        if (remoteResourceVersionStr.empty() || !std::all_of(remoteResourceVersionStr.begin(), remoteResourceVersionStr.end(), ::isdigit)) {
            remoteResourceVersionStr = localResourceVersionStr; // Evitar actualización
        }

        int remoteExeVersion = std::stoi(remoteExeVersionStr);
        int remoteResourceVersion = std::stoi(remoteResourceVersionStr);

        // Verificar si se necesitan actualizaciones
        bool needExeUpdate = localExeVersion < remoteExeVersion;
        bool needResourceUpdate = localResourceVersion < remoteResourceVersion;

        // Si no se necesitan actualizaciones, salir sin mostrar la consola
        if (!needExeUpdate && !needResourceUpdate) {
            if (MostrarDetallesConsola) {
                ShowMessage("No se necesitan actualizaciones.");
            }
            return;
        }

        // Asignar una consola para mostrar mensajes ya que hay actualizaciones
        if (!AllocConsole()) {
            LogError("No se pudo asignar la consola.");
            return;
        }

        // Redirigir stdout a la consola
        FILE* fp;
        if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0) {
            LogError("No se pudo redirigir stdout a la consola.");
            FreeConsole();
            return;
        }
        setvbuf(stdout, NULL, _IONBF, 0); // Deshabilitar el almacenamiento en búfer

        // Sincronizar E/S de C++ con E/S de C
        std::ios::sync_with_stdio(false);

        // Forzar la aparición de la consola
        HWND hConsole = GetConsoleWindow();
        if (hConsole != NULL) {
            ShowWindow(hConsole, SW_SHOW);
        }
        else {
            LogError("No se pudo obtener el manejador de la consola.");
        }

        // Deshabilitar la cruz para cerrar el programa mientras se está actualizando
        if (hConsole != NULL) {
            HMENU hMenu = GetSystemMenu(hConsole, FALSE);
            if (hMenu != NULL) {
                DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
            }
        }

        // Mensaje de inicio
        if (MostrarDetallesConsola) {
            ShowMessage("Iniciando proceso de actualizacion del cliente...");
        }

        // Mostrar las versiones para depuración
        if (ModoDebug) {
            ShowMessage("Version actual ejecutable: " + localExeVersionStr);
            ShowMessage("Version actual recursos: " + localResourceVersionStr);
            ShowMessage("Version remota del ejecutable del servidor: " + remoteExeVersionStr);
            ShowMessage("Version remota de recursos del servidor: " + remoteResourceVersionStr);
        }
        else {
            if (MostrarDetallesConsola) {
                ShowMessage("Verificando actualizaciones...");
            }
        }

        // Variable para rastrear si el ejecutable fue actualizado
        bool exeUpdated = false;

        // Actualizar ejecutable si es necesario
        if (needExeUpdate) {
            if (UpdateExecutable(appPath, remoteExeVersion)) {
                exeUpdated = true; // El ejecutable fue actualizado

                // Actualizar el archivo de versión local
                std::ofstream versionFile(localExeVersionFile);
                if (versionFile) {
                    versionFile << remoteExeVersion;
                    versionFile.close();
                }
                else {
                    LogError("No se pudo actualizar el archivo de version local: " + localExeVersionFile);
                }
            }
            else {
                LogError("Error al actualizar el ejecutable.");
            }
        }
        else {
            if (MostrarDetallesConsola) {
                ShowMessage("El ejecutable del juego se encuentra actualizado.");
            }
        }

        // Actualizar recursos si es necesario
        if (needResourceUpdate) {
            if (UpdateResources(appPath, localResourceVersion, remoteResourceVersion)) {
                // Actualización de recursos exitosa
            }
            else {
                LogError("Error al actualizar recursos.");
            }
        }
        else {
            if (MostrarDetallesConsola) {
                ShowMessage("Los recursos se encuentran actualizados.");
            }
        }

        // Mostrar el mensaje después de que todos los recursos hayan sido descargados y si el ejecutable fue actualizado
        if (exeUpdated) {
            // Mostrar el mensaje indicando que debe reiniciar el juego
            MessageBoxA(NULL, "El juego fue actualizado. Ahora debes abrir el juego nuevamente para disfrutar de la última versión de AOForever que se acaba de actualizar.", "Actualización Completa", MB_OK | MB_ICONINFORMATION);

            // Cerrar el proceso si hubo actualización del ejecutable
            ExitProcess(0);
        }
        else if (needResourceUpdate) {
            // Si solo se actualizan los recursos, mostrar mensaje y ocultar la consola
           // MessageBoxA(NULL, "Los recursos se han actualizado correctamente.", "Actualización AOForever Completada", MB_OK | MB_ICONINFORMATION);

            // Ocultar la consola en lugar de cerrar el proceso
            HWND hConsole = GetConsoleWindow();
            if (hConsole != NULL) {
                ShowWindow(hConsole, SW_HIDE);  // Ocultar la consola
            }

            // El proceso continúa sin cerrarse
        }

        // Liberar la consola (la consola del actualizador se cerrará)
        FreeConsole();
    }
    catch (const std::exception& ex) {
        LogError("Excepción: " + std::string(ex.what()));
        FreeConsole();
    }
    catch (...) {
        LogError("Ocurrió un error desconocido durante el parche.");
        FreeConsole();
    }
}

// Definición de la API exportada con convención de llamada stdcall
extern "C" __declspec(dllexport) void __stdcall CheckUpdates(const char* appPath)
{
    // Ejecutar el proceso de actualización directamente en el hilo principal
    std::string appPathStr = appPath;
    UpdateProcess(appPathStr);
}
