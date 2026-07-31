from tkinter import *
from PIL import Image, ImageTk, ImageDraw, ImageFont
import RPi.GPIO as GPIO
import time
import sys
import signal

# ================= KONFIGURASI GPIO =================
BUTTON_PIN1 = 27
BUTTON_PIN2 = 22
OUTPUT_PIN  = 17

GPIO.setmode(GPIO.BCM)
GPIO.setup(BUTTON_PIN1, GPIO.IN)
GPIO.setup(BUTTON_PIN2, GPIO.IN)
GPIO.setup(OUTPUT_PIN, GPIO.OUT)

# ================= KONFIGURASI TAMPILAN =================
BG_MAIN_PATH  = "/home/aria/Desktop/atmsampah-v1-irsensor-2026/bg_main.jpg"
BG_FRAME_PATH = "/home/aria/Desktop/atmsampah-v1-irsensor-2026/bg_frame.jpg"

WINDOW_W = 1024
WINDOW_H = 600

last_state = None


def signal_handler(signum, frame):
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
    global button1Label, button2Label, outputLabel

    root = Tk()
    root.geometry(f"{WINDOW_W}x{WINDOW_H}")
    root.resizable(False, False)
    root.title("UB | GC")
    root.config(bg="white")
    root.update()

    font_large = ImageFont.load_default()
    font_small = ImageFont.load_default()

    # === BACKGROUND MAIN ===
    bg_main_img = None
    try:
        bg_main_img = Image.open(BG_MAIN_PATH)
        bg_main_img = bg_main_img.resize((WINDOW_W, WINDOW_H), Image.LANCZOS)
        draw_main = ImageDraw.Draw(bg_main_img)
        title_text = "UB GC | ILLITERLESS"
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

    # === CARD KIRI: STATUS BUTTON 1 & 2 ===
    buttonFrame = Frame(mainFrame, bg="white", width=card_w, height=card_h,
                         highlightbackground="blue", highlightthickness=5)
    buttonFrame.place(x=left_x, y=card_y)
    buttonFrame.pack_propagate(False)
    Label(buttonFrame, bg="white", text="SALDO", font=("Helvetica", 15, "bold")).place(
        relx=0.5, y=18, anchor=CENTER)

    Label(buttonFrame, bg="white", text="Botol Masuk", font=("Helvetica", 11, "bold")).place(x=20, y=70)
    button1Label = Label(buttonFrame, bg="white", text="TIDAK", font=("Helvetica", 14, "bold"), fg="red")
    button1Label.place(x=20, y=95)

    Label(buttonFrame, bg="white", text="Tutup Botol Masuk", font=("Helvetica", 11, "bold")).place(x=20, y=140)
    button2Label = Label(buttonFrame, bg="white", text="TIDAK", font=("Helvetica", 14, "bold"), fg="red")
    button2Label.place(x=20, y=165)

    # === CARD KANAN: STATUS OUTPUT ===
    outputFrame = Frame(mainFrame, bg="white", width=card_w, height=card_h,
                         highlightbackground="red", highlightthickness=5)
    outputFrame.place(x=right_x, y=card_y)
    outputFrame.pack_propagate(False)
    Label(outputFrame, bg="white", text="Detail Transaksi", font=("Helvetica", 15, "bold")).place(
        relx=0.5, y=18, anchor=CENTER)
    Label(outputFrame, bg="white", text="Trx Id Status", font=("Helvetica", 12, "bold")).place(
        relx=0.5, y=90, anchor=CENTER)
    outputLabel = Label(outputFrame, bg="white", text="TIDAK AKTIF", font=("Helvetica", 22, "bold"), fg="red")
    outputLabel.place(relx=0.5, y=135, anchor=CENTER)

    # === TOMBOL KELUAR ===
    btn_w = 150
    btn_h = 50
    btn_x = left_x + (total_w - btn_w) // 2
    btn_y = card_y + card_h + 18

    if bg_frame_img:
        btn_bg = getPixelColor(bg_frame_img, btn_x + btn_w // 2, btn_y + btn_h // 2)
    else:
        btn_bg = "white"

    makeBtn(mainFrame, "Keluar", "#a01a1a", "#cc2828", closeWindow, btn_x, btn_y, btn_w, btn_h, bg_color=btn_bg)

    mainFrame.lift()
    stampFrame.lift()
    buttonFrame.lift()
    outputFrame.lift()

    updateTime()
    updateDate()


def updateTime():
    timeStamp.config(text=time.strftime("%H:%M:%S"))
    timeStamp.after(1000, updateTime)


def updateDate():
    dateStamp.config(text=time.strftime("%d-%m-%Y"))
    dateStamp.after(86400000, updateDate)


def pollButtons():
    global last_state

    state1 = GPIO.input(BUTTON_PIN1)
    state2 = GPIO.input(BUTTON_PIN2)
    output = state1 and state2

    button1Label.config(text="YA" if state1 else "TIDAK", fg="green" if state1 else "red")
    button2Label.config(text="YA" if state2 else "TIDAK", fg="green" if state2 else "red")

    if output != last_state:
        last_state = output
        if output:
            outputLabel.config(text="AKTIF", fg="green")
            print("Button 1 dan 2 ditekan")
        else:
            outputLabel.config(text="TIDAK AKTIF", fg="red")
            print("Salah satu button dilepas")

    GPIO.output(OUTPUT_PIN, output)
    root.after(50, pollButtons)


def closeWindow():
    GPIO.cleanup()
    root.destroy()


mainPage()
root.after(50, pollButtons)
root.protocol("WM_DELETE_WINDOW", closeWindow)
root.mainloop()
