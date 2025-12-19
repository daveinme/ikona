# Nomi Icone Applicazione IKONA

## Dimensioni Consigliate
- **Pulsanti Navbar (Top-Bar)**: 37x37px quadrate (pulsanti 40x40px)
- **Pulsanti Sidebar**: 37x37px quadrate (pulsanti 40x40px)
- **Icone Generali**: SVG per scalabilità o PNG 37x37px

---

## Navbar (Top-Bar) - 4 Icone
Posizione: `/icons/`

| Nome File | Utilizzo | Stato |
|-----------|----------|-------|
| `importa_icon.png` | Pulsante "Importa" nella navbar | ⏳ Da creare |
| `foto_icon.png` | Pulsante "Foto" (View) nella navbar | ⏳ Da creare |
| `stampa_icon.png` | Pulsante "Stampa" nella navbar | ⏳ Da creare |
| `config_icon.png` | Pulsante "Config" nella navbar | ⏳ Da creare |

---

## Sidebar Importa (Import Tab)

| Nome File | Utilizzo | Nota |
|-----------|----------|------|
| `importa_sidebar_icon.png` | Pulsante "Importa" manuale | Opzionale |
| `sfoglia_icon.png` | Pulsante "Sfoglia..." (sorgente/destinazione) | Opzionale |
| `aggiorna_sd_icon.png` | Pulsante "Aggiorna SD" | Opzionale |

---

## Sidebar View (View Tab)

| Nome File | Utilizzo | Nota |
|-----------|----------|------|
| `apri_icon.png` | Pulsante "Apri..." (cartella) | Opzionale |
| `pulisci_icon.png` | Pulsante "Pulisci" | Opzionale |
| `seleziona_tutto_icon.png` | Pulsante "Seleziona Tutto" | Opzionale |
| `deseleziona_icon.png` | Pulsante "Deseleziona Tutto" | Opzionale |
| `aggiorna_icon.png` | Pulsante "👁 Aggiorna" (mostra selezionate) | Opzionale |
| `editor_icon.png` | Pulsante "Editor" | Opzionale |
| `stampa_view_icon.png` | Pulsante "Stampa" in sidebar view | Opzionale |
| `monitor_icon.png` | Pulsante "Monitor Cliente" | Opzionale |
| `tutte_icon.png` | Pulsante "Tutte" (mostra tutte le foto, swap con selezionate) | Opzionale |

---

## Editor Tab

| Nome File | Utilizzo | Nota |
|-----------|----------|------|
| `brightness_dec_icon.png` | Pulsante diminuisci brightness | Opzionale |
| `brightness_inc_icon.png` | Pulsante aumenta brightness | Opzionale |
| `reset_icon.png` | Pulsante "Reset Filtri" | Opzionale |
| `rotate_left_icon.png` | Pulsante "Ruota Sinistra" (disabled) | Opzionale |
| `rotate_right_icon.png` | Pulsante "Ruota Destra" (disabled) | Opzionale |
| `prev_icon.png` | Pulsante "Precedente" (navigazione immagini) | Opzionale |
| `next_icon.png` | Pulsante "Successivo" (navigazione immagini) | Opzionale |
| `save_next_icon.png` | Pulsante "Salva & Successivo" | Opzionale |
| `cancel_icon.png` | Pulsante "Annulla" | Opzionale |
| `save_all_icon.png` | Pulsante "Salva Tutti" | Opzionale |

---

## Sidebar Stampa (Print Tab)

| Nome File | Utilizzo | Nota |
|-----------|----------|------|
| `stampa_tutto_icon.png` | Pulsante "🖨 Stampa Tutto" | Opzionale |
| `pulisci_print_icon.png` | Pulsante "🗑 Pulisci" (coda stampa) | Opzionale |

---

## Note Implementazione

1. **Fallback Automatico**: Se l'icona non esiste, il pulsante mostra automaticamente il testo emoji
2. **Quadratura**: Tutte le icone devono essere **quadrate** (stesso larghezza e altezza)
3. **Dimensione**: L'icona deve essere 37x37px per riempire quasi completamente il pulsante 40x40px
4. **Colore**: Le icone vengono visualizzate come-è, senza colorazione GTK
5. **Sfondo**: Icone con sfondo trasparente (.png con alpha)

---

## Priorità Creazione

**Alta (Essenziali)**:
- ✅ `importa_icon.png`
- ✅ `foto_icon.png`
- ✅ `stampa_icon.png`
- ✅ `config_icon.png`

**Media (Consigliati)**:
- `pulisci_icon.png`
- `seleziona_tutto_icon.png`
- `aggiorna_icon.png`
- `editor_icon.png`
- `stampa_tutto_icon.png`

**Bassa (Opzionali)**:
- Tutti gli altri

---

## Formato Consigliato

- **Formato**: PNG (256 colori o RGB+Alpha)
- **Dimensione**: 24x24px (quadrato perfetto)
- **DPI**: 96
- **Sfondo**: Trasparente
- **Stile**: Monocromatico giallo (#ffbd04) o colorato secondo design

---

## Esempio di Utilizzo nel Codice

```c
/* Nel main() di ikona.c */
char importa_icon_path[1024];
snprintf(importa_icon_path, sizeof(importa_icon_path), "%s/importa_icon.png", icons_dir);
btn_import = create_icon_button(importa_icon_path, "📥 Importa");
```

Se il file non esiste, il pulsante mostrerà l'emoji fallback "📥 Importa".

---

## Posizionamento File

```
ikona/
├── icons/
│   ├── importa_icon.png      ← DA AGGIUNGERE
│   ├── foto_icon.png         ← DA AGGIUNGERE
│   ├── stampa_icon.png       ← DA AGGIUNGERE
│   ├── config_icon.png       ← DA AGGIUNGERE
│   └── ... (altri se create)
└── ...
```
