CREATE TABLE authors (
    author_id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    country VARCHAR(50)
);

CREATE TABLE genres (
    genre_id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

CREATE TABLE publishers (
    publisher_id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    city VARCHAR(50)
);

CREATE TABLE languages (
    language_id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

CREATE TABLE books (
    book_id SERIAL PRIMARY KEY,
    title VARCHAR(150) NOT NULL,
    author_id INT REFERENCES authors(author_id),
    genre_id INT REFERENCES genres(genre_id),
    publisher_id INT REFERENCES publishers(publisher_id),
    language_id INT REFERENCES languages(language_id),
    year INT,
    pages INT,
    copies_total INT DEFAULT 1,
    copies_available INT DEFAULT 1
);

CREATE TABLE readers (
    reader_id SERIAL PRIMARY KEY,
    full_name VARCHAR(100) NOT NULL,
    phone VARCHAR(20),
    email VARCHAR(100)
);

CREATE TABLE loans (
    loan_id SERIAL PRIMARY KEY,
    book_id INT REFERENCES books(book_id),
    reader_id INT REFERENCES readers(reader_id),
    loan_date DATE NOT NULL,
    return_date DATE
);

INSERT INTO authors (name, country) VALUES
('Достоевский Ф.М.', 'Россия'),
('Толстой Л.Н.', 'Россия'),
('Оруэлл Дж.', 'Великобритания'),
('Кинг С.', 'США');

INSERT INTO genres (name) VALUES
('Роман'),
('Фантастика'),
('Детектив'),
('Ужасы');

INSERT INTO publishers (name, city) VALUES
('Эксмо', 'Москва'),
('Питер', 'Санкт-Петербург'),
('АСТ', 'Москва');

INSERT INTO languages (name) VALUES
('Русский'),
('English'),
('Deutsch'),
('Français');

INSERT INTO books (title, author_id, genre_id, publisher_id, language_id, year, pages, copies_total, copies_available) VALUES
('Преступление и наказание', 1, 1, 1, 1, 1866, 550, 3, 1),
('Война и мир',               2, 1, 3, 1, 1869, 1225, 5, 2),
('1984',                      3, 2, 2, 2, 1949, 320, 4, 0),
('Сияние',                    4, 4, 1, 2, 1977, 447, 2, 1);

INSERT INTO readers (full_name, phone, email) VALUES
('Иванов Иван',   '123456', 'ivan@mail.ru'),
('Петров Петр',   '654321', 'petr@mail.ru'),
('Сидорова Анна', '777888', 'anna@mail.ru');

INSERT INTO loans (book_id, reader_id, loan_date, return_date) VALUES
(1, 1, '2024-01-10', NULL),
(3, 2, '2024-01-12', '2024-01-20'),
(4, 3, '2024-02-01', NULL);
