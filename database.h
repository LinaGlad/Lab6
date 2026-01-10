#pragma once
#include <libpq-fe.h>
#include <string>

// Базовые функции
void checkConn(PGconn* conn);
void printResult(PGresult* res);
void execAndPrint(PGconn* conn, const char* query, int nParams, const char* const* params);

// Основные операции с книгами/выдачами
void listBooks(PGconn* conn);
void listActiveLoans(PGconn* conn);
void addReader(PGconn* conn);
void addBook(PGconn* conn);
void loanBook(PGconn* conn);
void returnBook(PGconn* conn);
void deleteBook(PGconn* conn);

// Фильтры по книгам
void booksByYear(PGconn* conn);
void booksByPublisher(PGconn* conn);
void booksByGenre(PGconn* conn);
void booksByPages(PGconn* conn);
void booksByAuthor(PGconn* conn);
void freeBooks(PGconn* conn);

// Читатели
void listReaders(PGconn* conn);
void readerLoans(PGconn* conn);

// Справочная информация и добавление сущностей
void addAuthor(PGconn* conn);
void addGenre(PGconn* conn);
void addPublisher(PGconn* conn);
void addLanguage(PGconn* conn);
void listReferenceData(PGconn* conn);
