/*
 * Copyright (C) 2026 TermoGRAF
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <format>
#include <ranges>
#include <algorithm>
#include <windows.h>

namespace fs = std::filesystem;

// изменение времени создания, модификации и доступа
void apply_file_time_offset(const fs::path& file_path, ULONGLONG base_time_quad, size_t file_index) {
    HANDLE h_file = CreateFileW(
        file_path.c_str(), 
        FILE_WRITE_ATTRIBUTES, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL, 
        NULL
    );

    if (h_file != INVALID_HANDLE_VALUE) {
        ULARGE_INTEGER new_time_uli;
        
        // вычисляем новое время для file_index, уменьшая его на 60 секунд (600000000 в 100-наносекундных интервалах)
        new_time_uli.QuadPart = base_time_quad - (static_cast<ULONGLONG>(file_index) * 600000000ULL);
        
        FILETIME new_time;
        new_time.dwLowDateTime = new_time_uli.LowPart;
        new_time.dwHighDateTime = new_time_uli.HighPart;
        
        SetFileTime(h_file, &new_time, &new_time, &new_time);
        CloseHandle(h_file);
    }
}

// текущеe время в формате WinAPI
ULARGE_INTEGER get_current_system_filetime() {
    FILETIME current_ft;
    GetSystemTimeAsFileTime(&current_ft);
    ULARGE_INTEGER base_time;
    base_time.LowPart = current_ft.dwLowDateTime;
    base_time.HighPart = current_ft.dwHighDateTime;
    return base_time;
}

void print_progress(size_t current, size_t total) {
    constexpr int bar_width = 50;
    float progress = total > 0 ? static_cast<float>(current) / total : 1.0f;
    int pos = static_cast<int>(bar_width * progress);

    std::string bar;
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) bar += "=";
        else if (i == pos) bar += ">";
        else bar += " ";
    }

    std::cout << std::format("\r[{}] {}/{} ({}%)", bar, current, total, static_cast<int>(progress * 100.0f));
    std::cout << std::flush;
}

size_t get_total_files(const fs::path& target_path) {
    if (!fs::exists(target_path) || !fs::is_directory(target_path)) return 0;
    
    std::string dir_name = target_path.filename().string();
    if (dir_name == "System Volume Information" || dir_name == "$RECYCLE.BIN") return 0;

    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(target_path)) {
        if (fs::is_regular_file(entry)) {
            count++;
        } else if (fs::is_directory(entry)) {
            count += get_total_files(entry.path());
        }
    }
    return count;
}

void sort_directory_physically(const fs::path& target_path, size_t total_files, size_t& processed_files) {
    if (!fs::exists(target_path) || !fs::is_directory(target_path)) return;

    std::string dir_name = target_path.filename().string();
    if (dir_name == "System Volume Information" || dir_name == "$RECYCLE.BIN") return;

    std::vector<fs::path> files;
    std::vector<fs::path> subdirs;

    for (const auto& entry : fs::directory_iterator(target_path)) {
        if (fs::is_regular_file(entry)) {
            files.push_back(entry.path());
        } else if (fs::is_directory(entry)) {
            subdirs.push_back(entry.path());
        }
    }

    if (!files.empty()) {
        std::ranges::sort(files);

        fs::path temp_dir = target_path / "FAT32_SORT_TEMP_DIR";
        fs::create_directory(temp_dir);

        std::vector<fs::path> temp_files;
        temp_files.reserve(files.size());

        for (const auto& file : files) {
            fs::path temp_file = temp_dir / file.filename();
            fs::rename(file, temp_file);
            temp_files.push_back(temp_file);
        }
        const auto base_time = get_current_system_filetime();
        for (size_t i = 0; i < files.size(); ++i) {
            fs::rename(temp_files[i], files[i]);

            apply_file_time_offset(files[i], base_time.QuadPart, i);
            processed_files++;
            print_progress(processed_files, total_files);
        }

        fs::remove(temp_dir);
    }

    std::ranges::sort(subdirs);
    for (const auto& subdir : subdirs) {
        sort_directory_physically(subdir, total_files, processed_files);
    }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::string path_str;
    if (argc > 1) {
        path_str = argv[1];
    } else {
        std::cout << "========================================\n";
        std::cout << "    Сортировка FAT32 для ГУ HAVAL 1.0\n";
        std::cout << "      Copyright (C) 2026 TermoGRAF\n";
        std::cout << "========================================\n";
        std::cout << "Использование:\n";
        std::cout << "  haval_sort.exe <путь>\n";
        std::cout << "----------------------------------------\n";
        std::cout << "Введите букву флешки и путь к папке (например, F:\\)";
        return 0;
    }

    if (path_str.starts_with('"') && path_str.ends_with('"')) {
        path_str = path_str.substr(1, path_str.length() - 2);
    }
    fs::path target_path(path_str);
    
    try {
        std::cout << "Подсчет файлов...\n";
        size_t total_files = get_total_files(target_path);
        
        if (total_files == 0) {
            std::cout << "Файлы для сортировки не найдены.\n";
        } else {
            std::cout << std::format("Найдено файлов: {}\nНачинаю сортировку:\n", total_files);
            
            size_t processed_files = 0;
            print_progress(processed_files, total_files); 
            
            sort_directory_physically(target_path, total_files, processed_files);
            
            std::cout << "\n\nВсе операции успешно завершены!\n";
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << std::format("\n\nошибка доступа: {}\n", e.what());
        std::cerr << "Убедитесь, что флешка не используется другими программами.\n";
    }

    return 0;
}