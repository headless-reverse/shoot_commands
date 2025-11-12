# ⚡ shoot_commands
**app deskopowa do zarządzania i uruchamiania komend systemowych / workflow w C++ Qt6 (Linux)**

```bash
cmake -B build
cmake --build build -j$(nproc)
```
## 🖥️ Interfejs GUI
Categories  
Commands  
Log Output + LogDialog  
Controls  
Workflow – uruchamianie sekwencji JSON  

## ⏱️ Harmonogram komend
Wykonanie pojedyncze lub cykliczne **Periodic**  
Interwał ustawiany w sekundach **(1, 86400) 1 sek. do 24 godzin**  
QTimer kontroluje wykonywanie cykliczne  

## 💥 Uruchamianie komend
W trybie użytkownika: /bin/bash -c "komenda"  
W trybie root: /usr/bin/sudo -n bash -c "komenda"  
Ostrzeżenia przed komendami destrukcyjnymi (rm, dd, wipe, format, flashall)  
Safe Mode blokuje niebezpieczne operacje  

## 📝 Workflow
JSON workflow – lista komend do uruchomienia sekwencyjnie  
Obsługa wielu plików workflow (kolejka)  
Zapis JSON workflow:
```json
[
  {
    "command": "ls -l /root/",
    "delayAfterMs": 500,
    "runAsRoot": true,
    "stopOnError": true
  }
]
```

**command** – komenda do wykonania  
**delayAfterMs** – opóźnienie po wykonaniu komendy (ms)  
**runAsRoot** – czy wykonać jako root  
**stopOnError** – zatrzymać sekwencję jeśli komenda zakończy się błędem  

## ⚠️ Uprawnienia/root
Aplikacja tworzy katalog /usr/local/etc/shoot_commands/  
JSON /usr/local/etc/shoot_commands/shoot_commands.json  
Logi zapisywane w /usr/local/log/shoot_commands.log  
Aby zapisywać JSON uruchamiać jako root lub nadać prawa:  
```bash
  chmod -R 775 /usr/local/etc/shoot_commands/
  chown -R root:user /usr/local/etc/shoot_commands/
```
NOPASSWD sudo
```
  sudo visudo
```
Dodaj linię (zamień NAZWA_USER na konto):  
**NAZWA_USER ALL=(ALL) NOPASSWD: ALL**

## 🛠️ Struktura katalogów
```perl
├── /usr/local/
│   ├── bin/
│   │   └── shoot_commands
│   ├── etc/
│   │   └── shoot_commands/
│   │     	├── shoot_commands.json
│   │     	└── workflow.json
│   ├── log
│   │   └── shoot_commands.log
```
