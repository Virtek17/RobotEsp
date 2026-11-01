"""Deterministic wiring drawing for the GPIOs in the robot firmware."""
from pathlib import Path
from html import escape
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).parent
W, H, SCALE = 1280, 1300, 2
im = Image.new('RGB', (W*SCALE, H*SCALE), '#f5f7fb')
d = ImageDraw.Draw(im)
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
       '<rect width="100%" height="100%" fill="#f5f7fb"/>']
INK, RED, GND = '#17233b', '#d33b40', '#39465a'
BLUE, GREEN, PURPLE, ORANGE = '#2477ce', '#13855d', '#8055bd', '#c57800'

def text(x,y,s,size=20,color=INK,bold=False,anchor='start'):
    font = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf' if bold else 'C:/Windows/Fonts/segoeui.ttf',size*SCALE)
    width=d.textlength(s,font=font)
    px=x*SCALE - (width/2 if anchor=='middle' else width if anchor=='end' else 0)
    d.text((px,y*SCALE),s,font=font,fill=color)
    svg.append(f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}" font-family="Segoe UI, sans-serif" font-weight="{700 if bold else 400}" dominant-baseline="text-before-edge" text-anchor="{anchor}">{escape(s)}</text>')

def rect(x,y,w,h,fill='#ffffff',stroke='#d4dce8',r=16):
    d.rounded_rectangle((x*SCALE,y*SCALE,(x+w)*SCALE,(y+h)*SCALE),radius=r*SCALE,fill=fill,outline=stroke,width=2*SCALE)
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')

def wire(points,color,width=4):
    d.line([(x*SCALE,y*SCALE) for x,y in points],fill=color,width=width*SCALE)
    svg.append(f'<polyline points="'+ ' '.join(f'{x},{y}' for x,y in points)+f'" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"/>')

def dot(x,y,color,r=6):
    d.ellipse(((x-r)*SCALE,(y-r)*SCALE,(x+r)*SCALE,(y+r)*SCALE),fill=color)
    svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')

def pin(x,y,label,color,side='right'):
    dot(x,y,color)
    text(x-18 if side=='right' else x+18,y-15,label,21,color,True,'end' if side=='right' else 'start')

text(54,32,'МОЧИ · СХЕМА ПОДКЛЮЧЕНИЯ',35,bold=True)
text(54,85,'ESP32-C3 Super Mini + OLED SSD1306 + сенсорная кнопка + INMP441',22)
rect(54,132,1172,54,'#fff3db','#e7c883',10)
text(74,143,'Подключать при отключённом USB. Питание модулей: только 3,3 В.',22)

rect(54,250,356,860,'#e9eff8','#bac9df')
text(232,275,'ESP32-C3',29,bold=True,anchor='middle')
text(232,313,'Super Mini',22,anchor='middle')
rect(850,250,376,248)
text(880,271,'OLED 128×64 · SSD1306',24,bold=True)
rect(850,540,376,182)
text(880,558,'СЕНСОРНАЯ КНОПКА',23,bold=True)
rect(850,790,376,306)
text(880,807,'МИКРОФОН · INMP441',23,bold=True)

# Power distribution: junction dots alone indicate electrical connections.
wire([(640,218),(640,850)],RED,5)
wire([(690,218),(690,930)],GND,5)
text(640,199,'3,3 В',18,RED,True,'end')
text(700,199,'GND',18,GND,True)
wire([(410,340),(640,340)],RED)
wire([(410,380),(850,380)],GND)
pin(410,340,'3V3',RED); dot(640,340,RED)
pin(410,380,'GND',GND); dot(690,380,GND)
wire([(640,340),(850,340)],RED)
pin(850,340,'VDD',RED,'left'); pin(850,380,'GND',GND,'left')

for y,label,color,gpio in [(420,'SDA',BLUE,'GPIO20'),(460,'SCK / SCL',GREEN,'GPIO21'),
                            (680,'I/O',ORANGE,'GPIO3'),(970,'SD',BLUE,'GPIO4'),
                            (1010,'WS',PURPLE,'GPIO5'),(1050,'SCK',GREEN,'GPIO6')]:
    wire([(410,y),(850,y)],color)
    pin(410,y,gpio,color)
    pin(850,y,label,color,'left')

for y,label,color,bus in [(600,'VCC',RED,640),(640,'GND',GND,690),
                          (850,'VDD',RED,640),(890,'GND',GND,690),(930,'L/R',GND,690)]:
    wire([(bus,y),(850,y)],color)
    dot(bus,y,color)
    pin(850,y,label,color,'left')

text(80,535,'Подписаны номера GPIO.',20)
text(80,565,'Ориентируйтесь на надписи',20)
text(80,595,'на своей плате.',20)
text(80,765,'USB → ноутбук',24,bold=True)
text(80,807,'Питание и прошивка',19)
text(80,837,'Голос звучит через',19)
text(80,867,'колонки ноутбука.',19)

text(54,1132,'Важно',24,bold=True)
text(54,1170,'• SCK дисплея → GPIO21. SCK микрофона → GPIO6. Не соединять их вместе.',20)
text(54,1202,'• VDD микрофона — не 5 В. L/R соединить с общей землёй GND.',20)
text(54,1234,'• Если на INMP441 нет резистора SD–GND 100 кОм, добавьте его отдельно.',19)
text(54,1265,'Схематическое расположение контактов. Соединены только линии с точками в месте пересечения.',16,'#5c6980')

svg.append('</svg>')
(OUT/'mochi-wiring.svg').write_text('\n'.join(svg),encoding='utf-8')
im.save(OUT/'mochi-wiring.png')
print(OUT/'mochi-wiring.png')
