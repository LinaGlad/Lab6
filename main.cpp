#include <iostream>
#include <string>
#include <libpq-fe.h>
#include "database.h"

void showMainMenu() {
    std::cout << "===== МЕНЮ БИБЛИОТЕКИ =====\n";
    std::cout << "1. Книги\n";
    std::cout << "2. Выдачи\n";
    std::cout << "3. Читатели\n";
    std::cout << "4. Добавление справочной информации\n";
    std::cout << "5. Справочная информация (авторы, жанры, издательства, языки)\n";
    std::cout << "0. Выход\n";
    std::cout << "Выбор: ";
}

void booksMenu(PGconn* conn) {
    while (true) {
        std::cout << "\n===== КНИГИ =====\n";
        std::cout << "1. Просмотреть все книги\n";
        std::cout << "2. Добавить книгу\n";
        std::cout << "3. Удалить книгу / экземпляры\n";
        std::cout << "4. Поиск и фильтры\n";
        std::cout << "0. Назад\n";
        std::cout << "Выбор: ";

        int choice;
        std::string dummy;
        std::cin >> choice;
        std::getline(std::cin, dummy);

        if (choice == 0) break;

        switch (choice) {
        case 1: listBooks(conn);      break;
        case 2: addBook(conn);        break;
        case 3: deleteBook(conn);     break;
        case 4: {
            while (true) {
                std::cout << "\n===== ПОИСК И ФИЛЬТРЫ =====\n";
                std::cout << "1. Книги по году (<, >, =)\n";
                std::cout << "2. Книги по издательству\n";
                std::cout << "3. Книги по жанру\n";
                std::cout << "4. Книги по количеству страниц\n";
                std::cout << "5. Книги по автору\n";
                std::cout << "6. Свободные книги\n";
                std::cout << "0. Назад\n";
                std::cout << "Выбор: ";

                int fChoice;
                std::cin >> fChoice;
                std::getline(std::cin, dummy);

                if (fChoice == 0) break;

                switch (fChoice) {
                case 1: booksByYear(conn);      break;
                case 2: booksByPublisher(conn); break;
                case 3: booksByGenre(conn);     break;
                case 4: booksByPages(conn);     break;
                case 5: booksByAuthor(conn);    break;
                case 6: freeBooks(conn);        break;
                default:
                    std::cout << "Неверный выбор.\n";
                }
            }
            break;
        }
        default:
            std::cout << "Неверный выбор.\n";
        }
    }
}

void loansMenu(PGconn* conn) {
    while (true) {
        std::cout << "\n===== ВЫДАЧИ =====\n";
        std::cout << "1. Активные выдачи\n";
        std::cout << "2. Выдать книгу\n";
        std::cout << "3. Вернуть книгу\n";
        std::cout << "0. Назад\n";
        std::cout << "Выбор: ";

        int choice;
        std::string dummy;
        std::cin >> choice;
        std::getline(std::cin, dummy);

        if (choice == 0) break;

        switch (choice) {
        case 1: listActiveLoans(conn); break;
        case 2: loanBook(conn);        break;
        case 3: returnBook(conn);      break;
        default:
            std::cout << "Неверный выбор.\n";
        }
    }
}

void readersMenu(PGconn* conn) {
    while (true) {
        std::cout << "\n===== ЧИТАТЕЛИ =====\n";
        std::cout << "1. Все читатели (со статусом)\n";
        std::cout << "2. Книги конкретного читателя\n";
        std::cout << "3. Добавить читателя\n";
        std::cout << "0. Назад\n";
        std::cout << "Выбор: ";

        int choice;
        std::string dummy;
        std::cin >> choice;
        std::getline(std::cin, dummy);

        if (choice == 0) break;

        switch (choice) {
        case 1: listReaders(conn);  break;
        case 2: readerLoans(conn);  break;
        case 3: addReader(conn);    break;
        default:
            std::cout << "Неверный выбор.\n";
        }
    }
}

void addReferenceMenu(PGconn* conn) {
    while (true) {
        std::cout << "\n===== ДОБАВЛЕНИЕ СПРАВОЧНОЙ ИНФОРМАЦИИ =====\n";
        std::cout << "1. Добавить автора\n";
        std::cout << "2. Добавить жанр\n";
        std::cout << "3. Добавить издательство\n";
        std::cout << "4. Добавить язык\n";
        std::cout << "0. Назад\n";
        std::cout << "Выбор: ";

        int choice;
        std::string dummy;
        std::cin >> choice;
        std::getline(std::cin, dummy);

        if (choice == 0) break;

        switch (choice) {
        case 1: addAuthor(conn);     break;
        case 2: addGenre(conn);      break;
        case 3: addPublisher(conn);  break;
        case 4: addLanguage(conn);   break;
        default:
            std::cout << "Неверный выбор.\n";
        }
    }
}

int main() {
    PGconn* conn = PQconnectdb(
        "host=localhost port=5432 dbname=library user=postgres password=1234"
    );

    checkConn(conn);

    while (true) {
        showMainMenu();
        int choice;
        std::string dummy;
        std::cin >> choice;
        std::getline(std::cin, dummy);

        switch (choice) {
        case 1: booksMenu(conn);        break;
        case 2: loansMenu(conn);        break;
        case 3: readersMenu(conn);      break;
        case 4: addReferenceMenu(conn); break;
        case 5: listReferenceData(conn); break;
        case 0:
            PQfinish(conn);
            return 0;
        default:
            std::cout << "Неверный выбор.\n";
        }
        
        std::cout << std::endl;
    }
}

