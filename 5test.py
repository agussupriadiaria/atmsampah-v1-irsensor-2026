from tkinter import *
from PIL import Image, ImageTk, ImageDraw, ImageFont
import RPi.GPIO as GPIO
import time
import sys
import signal
import random
import qrcode
from datetime import datetime

# ================= KONFIGURASI GPIO =================
BUTTON_PIN1 = 27
BUTTON_PIN2 = 22 #tutup botol
OUTPUT_PIN  = 6  #botol
START_PIN = 5

GPIO.setmode(GPIO.BCM)
# IR sensor tipe umum (LM393) bersifat active-LOW: idle=HIGH, terdeteksi=LOW.
# Pakai pull-up agar pin tidak dianggap "aktif" palsu saat modul belum stabil/idle.
GPIO.setup(BUTTON_PIN1, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(BUTTON_PIN2, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.setup(OUTPUT_PIN, GPIO.OUT, initial=GPIO.HIGH)  # idle=HIGH (sesuai INPUT_PULLUP di Arduino)
GPIO.setup(START_PIN, GPIO.OUT, initial=GPIO.LOW)

# ================= KONFIGURASI TAMPILAN =================
BG_MAIN_PATH  = "/home/aria/Desktop/atmsampah-v1-irsensor-2026/bg_main.jpg"
BG_FRAME_PATH = "/home/aria/Desktop/atmsampah-v1-irsensor-2026/bg_frame.jpg"
SAVE_PATH     = "/home/aria/Desktop/atmsampah-v1-irsensor-2026/saveData.txt"

WINDOW_W = 1024
WINDOW_H = 600

# Karena tidak ada lagi lookup barcode produk, nilai poin per botol dibuat tetap.
POIN_PER_BOTOL = 50

last_state = None
start_busy = False

bottle = 0
saldo  = 0
trxId  = None


def signal_handler(signum, frame):
    try:
        closeWindow()
    except Exception:
        pass
    sys.exit()


signal.signal(signal.SIGINT, signal_handler)


def getPixelColor(img, x, y):
    r, g, b = img.getpixel((x, y))[:3]
    return f"#{r:02x}{g:02x}{b:02x}"


def getLuminance(img, x, y):
    r, g, b = img.getpixel((x, y))[:3]
    return 0.299 * r + 0.587 * g + 0.114 * b


def makeBtn(parent, text, color, hover_color, cmd, x, y, w=110, h=50, bg_color="white"):
    cvs = Canvas(parent, width=w, height=h, bd=0, highlightthickness=0, bg=bg_color)
    cvs.place(x=x, y=y)

    def draw(c):
        img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        d.rounded_rectangle([0, 0, w, h], radius=10, fill=c)
        fnt = ImageFont.load_default()
        bbox = d.textbbox((0, 0), text, font=fnt)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        d.text(((w - tw) // 2, (h - th) // 2), text, font=fnt, fill="white")
        tk_img = ImageTk.PhotoImage(img)
        cvs.tk_img = tk_img
        cvs.delete("all")
        cvs.create_image(0, 0, anchor=NW, image=tk_img)

    def on_enter(e):
        draw(hover_color)

    def on_leave(e):
        draw(color)

    def on_press(e):
        cvs.place(x=x + 3, y=y + 3)
        draw(color)

    def on_release(e):
        cvs.place(x=x, y=y)
        draw(hover_color)
        cmd()

    draw(color)
    cvs.bind("<Enter>", on_enter)
    cvs.bind("<Leave>", on_leave)
    cvs.bind("<Button-1>", on_press)
    cvs.bind("<ButtonRelease-1>", on_release)
    return cvs


def mainPage():
    global root, timeStamp, dateStamp
    global saldoLabel, trxIdLabel, jumlahLabel, statusLabel

    root = Tk()
    root.geometry(f"{WINDOW_W}x{WINDOW_H}")
    root.resizable(False, False)
    root.title("ATM Sampah - PilahSampah")
    root.config(bg="white")
    root.update()

    font_large = ImageFont.load_default()

    # === BACKGROUND MAIN ===
    bg_main_img = None
    try:
        bg_main_img = Image.open(BG_MAIN_PATH)
        bg_main_img = bg_main_img.resize((WINDOW_W, WINDOW_H), Image.LANCZOS)
        draw_main = ImageDraw.Draw(bg_main_img)
        title_text = "PILAHSAMPAH"
        bbox = draw_main.textbbox((0, 0), title_text, font=font_large)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        tx = (WINDOW_W - tw) // 2
        ty = int(WINDOW_H * 0.075) - th // 2
        lum = getLuminance(bg_main_img, WINDOW_W // 2, int(WINDOW_H * 0.075))
        draw_main.text((tx, ty), title_text, font=font_large, fill="black" if lum > 128 else "white")
        bg_main_tk = ImageTk.PhotoImage(bg_main_img)
        bg_main_label = Label(root, image=bg_main_tk)
        bg_main_label.image = bg_main_tk
        bg_main_label.place(x=0, y=0, relwidth=1, relheight=1)
    except Exception as e:
        print(f"[BG] Gagal load {BG_MAIN_PATH}: {e}")
        root.config(bg="#dddddd")

    # === MAIN FRAME ===
    mainFrame = Frame(root, bd=10, highlightbackground="green", highlightthickness=5)
    mainFrame.place(relx=0.025, rely=0.15, relwidth=0.95, relheight=0.82)
    root.update()
    frame_w = mainFrame.winfo_width()
    frame_h = mainFrame.winfo_height()

    # === BACKGROUND FRAME ===
    bg_frame_img = None
    try:
        bg_frame_img = Image.open(BG_FRAME_PATH)
        bg_frame_img = bg_frame_img.resize((frame_w, frame_h), Image.LANCZOS)
        bg_frame_tk = ImageTk.PhotoImage(bg_frame_img)
        bg_frame_label = Label(mainFrame, image=bg_frame_tk, bd=0, highlightthickness=0)
        bg_frame_label.image = bg_frame_tk
        bg_frame_label.place(x=0, y=0, relwidth=1, relheight=1)
    except Exception as e:
        print(f"[BG] Gagal load {BG_FRAME_PATH}: {e}")
        mainFrame.config(bg="white")

    # === STAMP kiri atas (waktu & tanggal) ===
    if bg_frame_img:
        stamp_bg = getPixelColor(bg_frame_img, 30, 30)
        lum_s = getLuminance(bg_frame_img, 30, 30)
    else:
        stamp_bg = "#333333"
        lum_s = 0
    stamp_fg = "black" if lum_s > 128 else "white"
    stampFrame = Frame(mainFrame, bg=stamp_bg, bd=0)
    stampFrame.place(x=10, y=8)
    Label(stampFrame, text="Waktu  ", font=("Helvetica", 10, "bold"), bg=stamp_bg, fg=stamp_fg).grid(
        row=0, column=0, sticky="w", padx=(5, 0), pady=(3, 0))
    Label(stampFrame, text="Tanggal", font=("Helvetica", 10, "bold"), bg=stamp_bg, fg=stamp_fg).grid(
        row=1, column=0, sticky="w", padx=(5, 0), pady=(0, 3))
    timeStamp = Label(stampFrame, text="00:00:00", font=("Helvetica", 10, "bold"), bg=stamp_bg, fg=stamp_fg)
    timeStamp.grid(row=0, column=1, padx=(5, 5), pady=(3, 0))
    dateStamp = Label(stampFrame, text="dd/mm/yy", font=("Helvetica", 10, "bold"), bg=stamp_bg, fg=stamp_fg)
    dateStamp.grid(row=1, column=1, padx=(5, 5), pady=(0, 3))

    # === UKURAN & POSISI CARD ===
    card_w = int(frame_w * 0.38)
    card_h = 210
    gap = int(frame_w * 0.04)
    total_w = card_w * 2 + gap
    card_y = (frame_h - card_h - 100) // 2 + 20
    left_x = (frame_w - total_w) // 2
    right_x = left_x + card_w + gap

    # === CARD KIRI: TOTAL SALDO (gaya 3atmsampah) ===
    saldoFrame = Frame(mainFrame, bg="white", width=card_w, height=card_h,
                        highlightbackground="blue", highlightthickness=5)
    saldoFrame.place(x=left_x, y=card_y)
    saldoFrame.pack_propagate(False)
    Label(saldoFrame, bg="white", text="TOTAL SALDO", font=("Helvetica", 15, "bold")).place(
        relx=0.5, y=18, anchor=CENTER)
    Label(saldoFrame, text="Poin", font=("Helvetica", 28, "bold"), bg="white").place(
        relx=0.28, rely=0.6, anchor=CENTER)
    saldoLabel = Label(saldoFrame, text="0", font=("Helvetica", 28, "bold"), bg="white")
    saldoLabel.place(relx=0.65, rely=0.6, anchor=CENTER)

    # === CARD KANAN: DATA (gaya 3atmsampah, tanpa Ukuran/Barcode produk) ===
    dataFrame = Frame(mainFrame, bg="white", width=card_w, height=card_h,
                       highlightbackground="red", highlightthickness=5)
    dataFrame.place(x=right_x, y=card_y)
    dataFrame.pack_propagate(False)
    Label(dataFrame, bg="white", text="DATA", font=("Helvetica", 15, "bold")).place(
        relx=0.5, y=18, anchor=CENTER)
    for i, txt in enumerate(["TID", "Jumlah Botol", "Status Sensor"]):
        Label(dataFrame, bg="white", text=txt, font=("Helvetica", 11, "bold")).place(x=20, y=55 + i * 40)
    trxIdLabel = Label(dataFrame, bg="white", text="-----", font=("Helvetica", 14, "bold"))
    trxIdLabel.place(x=170, y=53)
    jumlahLabel = Label(dataFrame, bg="white", text="0", font=("Helvetica", 14, "bold"))
    jumlahLabel.place(x=170, y=93)
    statusLabel = Label(dataFrame, bg="white", text="TIDAK AKTIF", font=("Helvetica", 14, "bold"), fg="red")
    statusLabel.place(x=170, y=133)

    # === TOMBOL ===
    btn_w = 110
    btn_h = 50
    spacing = 16
    btn_y = card_y + card_h + 18

    if bg_frame_img:
        btn_bg = getPixelColor(bg_frame_img, left_x + total_w // 2, btn_y + btn_h // 2)
    else:
        btn_bg = "white"

    total_btn_w = btn_w * 2 + spacing * 1
    start_x0 = left_x + (total_w - total_btn_w) // 2
    mulai_x = start_x0
    estruk_x = mulai_x + btn_w + spacing

    makeBtn(mainFrame, "Mulai", "#1a7f37", "#28a745", startPulse, mulai_x, btn_y, btn_w, btn_h, bg_color=btn_bg)
    makeBtn(mainFrame, "E-Struk", "#b8860b", "#e0a721", resetCounter, estruk_x, btn_y, btn_w, btn_h, bg_color=btn_bg)

    mainFrame.lift()
    stampFrame.lift()
    saldoFrame.lift()
    dataFrame.lift()

    updateTime()
    updateDate()
    userIDNum()


# ================= LOGIKA TRANSAKSI (tanpa scan barcode & tanpa spreadsheet) =================

def userIDNum():
    """Buat TID baru untuk sesi/pelanggan berikutnya. Ditampilkan sejak posisi standby."""
    global trxId
    trxId = random.randrange(10000, 100000)
    trxIdLabel["text"] = str(trxId)


def bottleCounter():
    """Dipanggil saat sensor botol (tombol 1 & 2) aktif bersamaan -> dianggap 1 botol masuk."""
    global bottle, saldo
    bottle += 1
    saldo += POIN_PER_BOTOL
    jumlahLabel["text"] = bottle
    saldoLabel["text"] = saldo
    saveData()
    print(f"[Botol] Jumlah: {bottle}, Saldo: {saldo}, TID: {trxId}")


def resetCounter():
    """Tombol E-Struk: tampilkan QR untuk sesi berjalan, lalu reset untuk pelanggan berikutnya."""
    global bottle, saldo
    if bottle == 0:
        print("[E-Struk] Belum ada botol masuk, tidak ada struk untuk dicetak.")
        return
    showQRPopup()
    bottle = 0
    saldo = 0
    jumlahLabel["text"] = bottle
    saldoLabel["text"] = saldo
    userIDNum()
    print("Reset Jumlah Botol dan Saldo untuk sesi baru")


def showQRPopup():
    date_now = datetime.now().strftime("%d/%m/%Y")
    url = f"https://pilahsampah.com/transaction/?code={trxId}&date={date_now}&point={saldo}"
    qr = qrcode.QRCode(box_size=6, border=4)
    qr.add_data(url)
    qr.make(fit=True)
    qr_img = qr.make_image(fill_color="black", back_color="white")
    overlay = Frame(root, bg="white", bd=5, highlightbackground="black", highlightthickness=2)
    overlay.place(relx=0.5, rely=0.5, anchor=CENTER, width=400, height=460)
    overlay.lift()
    Label(overlay, text="* Scan kode ini dari HP untuk klaim poin", font=("Helvetica", 11, "bold"), bg="white").pack(pady=(20, 0))
    Label(overlay, text="* Kode akan hilang dalam 20 detik", font=("Helvetica", 11, "bold"), fg="red", bg="white").pack(pady=(0, 0))
    tk_img = ImageTk.PhotoImage(qr_img)
    qr_label = Label(overlay, image=tk_img, bg="white")
    qr_label.image = tk_img
    qr_label.pack(pady=10)
    Button(overlay, text="Tutup", font=("Helvetica", 10, "bold"), bg="red", fg="white", width=10,
           command=overlay.destroy).pack(pady=15)
    overlay.after(20000, overlay.destroy)
    print(f"[QR] URL: {url}")


def saveData():
    time_now = datetime.now().strftime("%H:%M:%S")
    date_now = datetime.now().strftime("%Y-%m-%d")
    try:
        with open(SAVE_PATH, 'a') as fb:
            fb.write(f"{trxId} / {time_now} / {date_now} / {bottle} / {saldo}\n")
    except Exception as e:
        print(f"[saveData] Gagal menyimpan data lokal: {e}")


# ================= GPIO PULSE UNTUK TOMBOL MULAI =================

def startPulse():
    _startPulse(START_PIN)


def _startPulse(pin):
    global start_busy
    if start_busy:
        return
    start_busy = True
    GPIO.output(pin, GPIO.HIGH)
    root.after(500, lambda: stopPulse(pin))


def stopPulse(pin):
    global start_busy
    GPIO.output(pin, GPIO.LOW)
    start_busy = False


# ================= JAM & TANGGAL =================

def updateTime():
    timeStamp.config(text=time.strftime("%H:%M:%S"))
    timeStamp.after(1000, updateTime)


def updateDate():
    dateStamp.config(text=time.strftime("%d-%m-%Y"))
    dateStamp.after(86400000, updateDate)


# ================= POLLING SENSOR BOTOL (pengganti scan barcode) =================

def pollButtons():
    global last_state

    state1 = GPIO.input(BUTTON_PIN1)
    state2 = GPIO.input(BUTTON_PIN2)
    # Sensor IR active-LOW: LOW berarti objek/botol terdeteksi.
    output = (state1 == GPIO.LOW) and (state2 == GPIO.LOW)

    if output != last_state:
        last_state = output
        if output:
            statusLabel.config(text="AKTIF", fg="green")
            bottleCounter()
        else:
            statusLabel.config(text="TIDAK AKTIF", fg="red")

    GPIO.output(OUTPUT_PIN, GPIO.LOW if output else GPIO.HIGH)  # LOW = trigger ke Arduino (INPUT_PULLUP)
    root.after(50, pollButtons)


def closeWindow():
    GPIO.output(OUTPUT_PIN, GPIO.HIGH)  # kembalikan ke idle (HIGH) sebelum keluar
    GPIO.output(START_PIN, GPIO.LOW)
    GPIO.cleanup()
    root.destroy()


mainPage()
root.after(50, pollButtons)
root.protocol("WM_DELETE_WINDOW", closeWindow)
root.mainloop()
