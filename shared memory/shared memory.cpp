#include <windows.h>
#include <iostream>
#include <string>
#include <thread>

const wchar_t SHARED_MEM_NAME[] = L"Global\\ChatSharedMemory";
const wchar_t SEMAPHORE_NAME_1[] = L"Global\\ChatSemaphore1";
const wchar_t SEMAPHORE_NAME_2[] = L"Global\\ChatSemaphore2";
const wchar_t MUTEX_NAME[] = L"Global\\ChatMutex";
const int BUF_SIZE = 512;
const int MESSAGE_SIZE = BUF_SIZE - 1;

HANDLE hMapFile = NULL;
HANDLE hSemaphore1 = NULL;
HANDLE hSemaphore2 = NULL;
HANDLE hMutex = NULL;
LPTSTR pBuf = NULL;

void Cleanup() {
    if (pBuf) UnmapViewOfFile(pBuf);
    if (hMapFile) CloseHandle(hMapFile);
    if (hSemaphore1) CloseHandle(hSemaphore1);
    if (hSemaphore2) CloseHandle(hSemaphore2);
    if (hMutex) CloseHandle(hMutex);
}

void ReceiverThread(bool isParty1) {
    while (true) {
        WaitForSingleObject(isParty1 ? hSemaphore2 : hSemaphore1, INFINITE);
        WaitForSingleObject(hMutex, INFINITE);

        if (pBuf[0] == 1) {
            std::wcout << (isParty1 ? L"Party2: " : L"Party1: ") << (pBuf + 1) << std::endl;
            pBuf[0] = 0;
        }

        ReleaseMutex(hMutex);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2 || (std::string(argv[1]) != "1" && std::string(argv[1]) != "2")) {
        std::cout << "Usage: " << argv[0] << " [1|2] (1 for party1, 2 for party2)" << std::endl;
        return 1;
    }

    bool isParty1 = std::string(argv[1]) == "1";

    // Инициализация с проверкой ошибок
    if (isParty1) {
        hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BUF_SIZE, SHARED_MEM_NAME);
        if (hMapFile == NULL) {
            std::cerr << "CreateFileMapping failed: " << GetLastError() << std::endl;
            return 1;
        }

        hSemaphore1 = CreateSemaphore(NULL, 1, 1, SEMAPHORE_NAME_1);
        if (hSemaphore1 == NULL) {
            std::cerr << "CreateSemaphore 1 failed: " << GetLastError() << std::endl;
            Cleanup();
            return 1;
        }

        hSemaphore2 = CreateSemaphore(NULL, 0, 1, SEMAPHORE_NAME_2);
        if (hSemaphore2 == NULL) {
            std::cerr << "CreateSemaphore 2 failed: " << GetLastError() << std::endl;
            Cleanup();
            return 1;
        }

        hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
        if (hMutex == NULL) {
            std::cerr << "CreateMutex failed: " << GetLastError() << std::endl;
            Cleanup();
            return 1;
        }
    }
    else {
        hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
        if (hMapFile == NULL) {
            std::cerr << "OpenFileMapping failed: " << GetLastError() << std::endl;
            return 1;
        }

        hSemaphore1 = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEMAPHORE_NAME_1);
        if (hSemaphore1 == NULL) {
            std::cerr << "OpenSemaphore 1 failed: " << GetLastError() << std::endl;
            Cleanup();
            return 1;
        }

        hSemaphore2 = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEMAPHORE_NAME_2);
        if (hSemaphore2 == NULL) {
            std::cerr << "OpenSemaphore 2 failed: " << GetLastError() << std::endl;
            Cleanup();
            return 1;
        }

        hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
        if (hMutex == NULL) {
            std::cerr << "OpenMutex failed: " << GetLastError() << std::endl;
            Cleanup();
            return 1;
        }
    }

    pBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, BUF_SIZE);
    if (pBuf == NULL) {
        std::cerr << "MapViewOfFile failed: " << GetLastError() << std::endl;
        Cleanup();
        return 1;
    }

    std::thread receiver(ReceiverThread, isParty1);
    receiver.detach();

    std::wcout << L"Chat started (Party" << (isParty1 ? L"1" : L"2") << L"). Type 'exit' to quit." << std::endl;

    std::wstring message;
    while (true) {
        std::getline(std::wcin, message);

        if (message == L"exit") {
            break;
        }

        WaitForSingleObject(hMutex, INFINITE);
        pBuf[0] = 1;
        wcscpy_s(pBuf + 1, MESSAGE_SIZE, message.c_str());
        ReleaseMutex(hMutex);

        ReleaseSemaphore(isParty1 ? hSemaphore1 : hSemaphore2, 1, NULL);
    }

    Cleanup();
    return 0;
}