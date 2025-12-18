# IKONA - Documentazione Progetto

## Panoramica
**Ikona** è un'applicazione desktop per la gestione e editing di foto, sviluppata in C con GTK3 per Linux e Windows.

---

## Tecnologie Utilizzate

### Stack Principale
- **Linguaggio:** C (C11)
- **GUI Framework:** GTK+-3.0
- **Build System:** Meson
- **Librerie:**
  - `gdk-pixbuf` - Manipolazione immagini
  - `gio` - I/O e filesystem
  - `glib` - Utility library
  - Windows API (`winspool.h`, `shell32.h`) - Supporto stampa Windows

---

## Struttura del Progetto

```
ikona/
├── ikona.c (1402 righe)          # Main - Logica principale applicazione
├── ikona.h                        # Header principale
├── editor.c (185 righe)          # Editor foto (rotazione implementata)
├── editor.h
├── viewer.c (592 righe)          # Visualizzazione griglia immagini
├── viewer.h
├── importer.c                     # Importazione da fotocamera/SD
├── importer.h
├── printer.c                      # Gestione stampa (Windows)
├── printer.h
├── icon_manager.c                # Gestione icone applicazione
├── icon_manager.h
├── utils.c                        # Funzioni utility
├── utils.h
├── style.css                      # Stile GTK personalizzato
├── meson.build                    # Configurazione build
└── icons/                         # Directory icone
```

---

## Architettura Applicazione

### Finestra Principale (ikona.c)
```
Main Window
│
├─ Tab Container (GTK Stack)
│  ├─ Import Tab (importer.c)
│  │  └─ Sidebar: Seleziona cartelle, auto-import SD
│  │
│  ├─ View Tab (viewer.c + editor.c)
│  │  ├─ Grid viewer (3x3 immagini per pagina)
│  │  ├─ Sidebar: Browse, Select, Filter, Edit, Print
│  │  └─ Editor window (finestra separata)
│  │     ├─ Image display (scrollable)
│  │     └─ Controls: Rotate, Crop, Save
│  │
│  ├─ Print Tab (printer.c)
│  │  ├─ Print queue
│  │  └─ Printer selection
│  │
│  └─ Config Tab
│     └─ Auto-import settings
│
└─ Secondary Viewer (viewer.c)
   └─ Finestra preview aggiuntiva
```

---

## Funzionalità Editor (editor.c)

### Stato Implementazione

| Funzione | Stato | Dettagli |
|----------|-------|----------|
| **Rotazione sinistra** | ✅ Implementata | `on_rotate_left()` - Ruota 270° |
| **Rotazione destra** | ✅ Implementata | `on_rotate_right()` - Ruota 90° |
| **Scaling** | ✅ Implementata | Ridimensionamento automatico |
| **Fullscreen** | ✅ Implementata | F11 per fullscreen |
| **Crop** | 🟡 Solo UI | Button presente, nessuna implementazione |
| **Save** | 🟡 Solo UI | Button presente, nessuna implementazione |
| **Brightness** | ❌ Non implementata | - |
| **Contrast** | ❌ Non implementata | - |
| **Saturation** | ❌ Non implementata | - |
| **Exposure** | ❌ Non implementata | - |
| **Highlights/Shadows** | ❌ Non implementata | - |

### Variabili di Stato Editor
```c
static GtkWidget *editor_window;       // Finestra editor
static GdkPixbuf *original_pixbuf;     // Immagine originale
static GdkPixbuf *current_pixbuf;      // Immagine modificata
static GtkWidget *image_display;       // Widget GTK per display
static char *current_image_path;       // Path immagine corrente
static gboolean is_cropping;           // Flag modalità crop
static int crop_start_x, crop_start_y; // Coordinate inizio crop
static int crop_end_x, crop_end_y;     // Coordinate fine crop
static GtkWidget *overlay;             // Overlay per disegno selezione
static GtkWidget *image_event_box;     // Event box per mouse events
```

---

## Flusso di Lavoro Editor

### 1. Apertura Editor
```
ikona.c:122 - on_edit_image_clicked()
  └─> editor.c:98 - create_photo_editor_window(image_path)
      ├─> Carica immagine con gdk_pixbuf_new_from_file()
      ├─> Crea finestra con layout horizontal box
      ├─> Sinistra: Image display con scroll
      └─> Destra: Controls sidebar (250px)
```

### 2. Applicazione Modifiche
```
Callback (es: on_rotate_left)
  ↓
Crea nuovo GdkPixbuf modificato
  ↓
Dealloca pixbuf vecchio (g_object_unref)
  ↓
Aggiorna current_pixbuf
  ↓
update_image_display()
  ↓
Scala immagine per fit window
  ↓
gtk_image_set_from_pixbuf()
  ↓
Refresh UI
```

### 3. Funzioni Chiave
- `create_photo_editor_window()` - Crea finestra editor
- `update_image_display()` - Aggiorna visualizzazione immagine
- `on_rotate_left()` / `on_rotate_right()` - Rotazione
- `on_editor_window_destroyed()` - Cleanup memoria

---

## API GdkPixbuf Disponibili

### Implementate
- `gdk_pixbuf_rotate_simple()` - Rotazione 90°/180°/270°
- `gdk_pixbuf_scale_simple()` - Ridimensionamento
- `gdk_pixbuf_new_from_file()` - Caricamento immagine

### Disponibili (non usate)
- `gdk_pixbuf_copy()` - Copia pixbuf
- `gdk_pixbuf_new_subpixbuf()` - Crop
- `gdk_pixbuf_flip()` - Flip orizzontale/verticale
- `gdk_pixbuf_get_pixels()` - Accesso diretto pixel
- `gdk_pixbuf_composite()` - Compositing
- Manipolazione diretta pixel per:
  - Brightness/Contrast
  - Saturation/Hue
  - Exposure
  - Highlights/Shadows
  - Color balance

---

## Pattern per Aggiungere Nuove Funzionalità

### Template Callback
```c
static void on_apply_filter(GtkButton *button, gpointer data) {
    if (!current_pixbuf) return;

    // 1. Copia pixbuf corrente
    GdkPixbuf *modified = gdk_pixbuf_copy(current_pixbuf);

    // 2. Applica trasformazione
    // ... manipolazione pixel ...

    // 3. Sostituisci pixbuf
    g_object_unref(current_pixbuf);
    current_pixbuf = modified;

    // 4. Aggiorna display
    update_image_display();
}
```

### Aggiunta Control UI
```c
// In create_photo_editor_window()
GtkWidget *button = gtk_button_new_with_label("Nome Filtro");
g_signal_connect(button, "clicked", G_CALLBACK(on_apply_filter), NULL);
gtk_box_pack_start(GTK_BOX(controls_box), button, FALSE, FALSE, 5);
```

---

## Note Tecniche

### Gestione Memoria
- **Original pixbuf** preservato per reset/undo
- **Current pixbuf** contiene modifiche progressive
- Cleanup automatico su chiusura (`on_editor_window_destroyed`)
- Usare `g_object_unref()` per deallocare GdkPixbuf

### Manipolazione Pixel
```c
// Accesso pixel
guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
int width = gdk_pixbuf_get_width(pixbuf);
int height = gdk_pixbuf_get_height(pixbuf);

// Iterazione pixel
for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
        guchar *p = pixels + y * rowstride + x * n_channels;
        // p[0] = Red, p[1] = Green, p[2] = Blue, p[3] = Alpha (se presente)
    }
}
```

---

## File Principali da Modificare

### Per aggiungere funzionalità editor:
1. **editor.c** - Implementazione funzioni editing
2. **editor.h** - Dichiarazioni funzioni pubbliche
3. **style.css** - (opzionale) Stile controlli UI

### Build
**IMPORTANTE**: La compilazione viene effettuata tramite **console MSYS2** (Windows).

```bash
# In MSYS2 console
meson setup build
meson compile -C build
```

---

## TODO / Funzionalità Mancanti

### Editor
- [ ] Implementare crop funzionale
- [ ] Implementare save/export
- [ ] Aggiungere brightness/contrast
- [ ] Aggiungere exposure control
- [ ] Aggiungere highlights/shadows
- [ ] Aggiungere saturation/vibrance
- [ ] Aggiungere color balance
- [ ] Aggiungere sharpening
- [ ] Aggiungere blur
- [ ] Implementare undo/redo stack

### UI
- [ ] Slider per controlli graduali
- [ ] Preview real-time
- [ ] Indicatori valori numerici
- [ ] Reset per singoli filtri

---

## Links Utili
- [GdkPixbuf Documentation](https://docs.gtk.org/gdk-pixbuf/)
- [GTK3 Documentation](https://docs.gtk.org/gtk3/)
