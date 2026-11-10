#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
字体位图生成器
从TTF字体文件生成用于STM32显示的字模C文件
支持中文字符和自定义字体大小
"""

import argparse
import os
import sys
from PIL import Image, ImageDraw, ImageFont
import numpy as np

def get_system_font_path():
    """
    获取系统默认宋体字体路径
    """
    system_paths = {
        'nt': [  # Windows
            'C:/Windows/Fonts/simsun.ttc',
            'C:/Windows/Fonts/simhei.ttf',
            'C:/Windows/Fonts/simkai.ttf',
        ],
        'posix': [  # Linux/macOS
            '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            '/System/Library/Fonts/PingFang.ttc',
            '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
        ]
    }

    paths = system_paths.get(os.name, [])
    for path in paths:
        if os.path.exists(path):
            return path

    # 如果没找到系统字体，返回None
    return None

def load_text_from_file(file_path):
    """
    从文本文件读取需要生成字模的文字
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read().strip()

        # 去重并保持顺序
        chars = []
        seen = set()
        for char in content:
            if char not in seen and char.strip():  # 跳过空白字符
                chars.append(char)
                seen.add(char)

        return chars
    except Exception as e:
        print(f"读取文本文件错误: {e}")
        return []

def char_to_bitmap(char, font, size):
    """
    将单个字符转换为位图数据
    """
    # 创建图像，大小稍大一些以避免字符被截断
    img_size = size + 8
    img = Image.new('L', (img_size, img_size), color=0)  # 黑色背景
    draw = ImageDraw.Draw(img)

    # 获取字符的边界框
    bbox = draw.textbbox((0, 0), char, font=font)
    char_width = bbox[2] - bbox[0]
    char_height = bbox[3] - bbox[1]

    # 计算居中位置
    x = (img_size - char_width) // 2 - bbox[0]
    y = (img_size - char_height) // 2 - bbox[1]

    # 绘制字符（白色）
    draw.text((x, y), char, fill=255, font=font)

    # 转换为numpy数组
    img_array = np.array(img)

    # 找到字符的实际边界
    rows = np.any(img_array > 128, axis=1)
    cols = np.any(img_array > 128, axis=0)

    if not np.any(rows) or not np.any(cols):
        # 如果没有找到字符，使用默认大小
        actual_width = size // 2
        actual_height = size
        bitmap = np.zeros((actual_height, actual_width), dtype=np.uint8)
    else:
        rmin, rmax = np.where(rows)[0][[0, -1]]
        cmin, cmax = np.where(cols)[0][[0, -1]]

        # 提取字符区域
        char_img = img_array[rmin:rmax+1, cmin:cmax+1]

        actual_height = char_img.shape[0]
        actual_width = char_img.shape[1]

        # 调整大小以符合指定尺寸（如果需要）
        if actual_height > size:
            # 等比例缩放
            scale = size / actual_height
            new_width = int(actual_width * scale)
            new_height = size

            img_resized = Image.fromarray(char_img).resize((new_width, new_height), Image.Resampling.LANCZOS)
            bitmap = np.array(img_resized)
            actual_width = new_width
            actual_height = new_height
        else:
            bitmap = char_img

    return bitmap, actual_width, actual_height

def bitmap_to_hex_data(bitmap, width, height):
    """
    将位图转换为十六进制字节数据
    """
    # 确保宽度是8的倍数（如果不是，填充到8的倍数）
    padded_width = ((width + 7) // 8) * 8

    # 创建填充后的位图
    padded_bitmap = np.zeros((height, padded_width), dtype=np.uint8)
    padded_bitmap[:height, :width] = bitmap

    # 将位图转换为二进制（阈值128）
    binary_bitmap = (padded_bitmap > 128).astype(np.uint8)

    hex_data = []
    bytes_per_row = padded_width // 8

    for row in range(height):
        for byte_col in range(bytes_per_row):
            byte_val = 0
            for bit in range(8):
                col = byte_col * 8 + bit
                if col < padded_width and binary_bitmap[row, col]:
                    byte_val |= (1 << (7 - bit))  # MSB first
            hex_data.append(f"0x{byte_val:02X}")

    return hex_data, padded_width

def generate_c_file(chars_data, output_file, array_name="font_data"):
    """
    生成C语言字模文件
    """
    lines = []

    # 文件头
    lines.append('#include "font.h"')
    lines.append('')
    lines.append(f'font_t {array_name}[] =')
    lines.append('{')

    for i, char_data in enumerate(chars_data):
        char = char_data['char']
        width = char_data['width']
        height = char_data['height']
        hex_data = char_data['hex_data']

        # 转义特殊字符
        if char == '"':
            char_str = '\\"'
        elif char == '\\':
            char_str = '\\\\'
        else:
            char_str = char

        lines.append('    {')
        lines.append(f'        .font = "{char_str}",')
        lines.append(f'        .width = {width},')
        lines.append(f'        .height = {height},')

        # 数据数组
        if hex_data:
            # 每行16个字节
            data_lines = []
            for j in range(0, len(hex_data), 16):
                line_data = hex_data[j:j+16]
                if j + 16 >= len(hex_data):
                    # 最后一行
                    data_lines.append('                  ' + ','.join(line_data) + ' }')
                else:
                    data_lines.append('                  ' + ','.join(line_data) + ',')

            lines.append('        .data = { ' + data_lines[0][18:])  # 去掉前面的空格
            for data_line in data_lines[1:]:
                lines.append(data_line)
        else:
            lines.append('        .data = { }')

        # 结构体结束
        if i == len(chars_data) - 1:
            lines.append('    }')
        else:
            lines.append('    },')

    lines.append('};')
    lines.append('')
    lines.append(f'const int {array_name}_count = {len(chars_data)};')

    # 写入文件
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        return True
    except Exception as e:
        print(f"写入文件错误: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description='从TTF字体生成STM32字模C文件，支持中文字符',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
示例用法:
  python font_generator.py -i font.txt -o font_32.c
  python font_generator.py -i font.txt -o font_24.c -s 24
  python font_generator.py -i font.txt -o font_16.c -s 16 -f arial.ttf
  python font_generator.py -i font.txt -o font_custom.c -s 32 -f "C:/Windows/Fonts/msyh.ttc"
        '''
    )

    parser.add_argument('-i', '--input', required=True,
                        help='输入文本文件路径（包含需要生成字模的文字）')
    parser.add_argument('-o', '--output', required=True,
                        help='输出C文件路径')
    parser.add_argument('-s', '--size', type=int, default=32,
                        help='字体大小（像素，默认32）')
    parser.add_argument('-f', '--font', default=None,
                        help='TTF字体文件路径（默认使用系统宋体）')
    parser.add_argument('-n', '--name', default=None,
                        help='生成的数组名称（默认根据字体大小自动生成）')

    args = parser.parse_args()

    # 显示信息
    print("=" * 60)
    print("STM32字体位图生成器")
    print("=" * 60)
    print(f"输入文件: {args.input}")
    print(f"输出文件: {args.output}")
    print(f"字体大小: {args.size}px")

    # 检查输入文件
    if not os.path.exists(args.input):
        print(f"错误: 输入文件 '{args.input}' 不存在")
        return 1

    # 读取文字
    chars = load_text_from_file(args.input)
    if not chars:
        print("错误: 没有找到有效的字符")
        return 1

    print(f"需要生成字模的字符: {''.join(chars)}")
    print(f"字符数量: {len(chars)}")

    # 确定字体文件
    font_path = args.font
    if not font_path:
        font_path = get_system_font_path()
        if not font_path:
            print("错误: 未找到系统字体，请使用 -f 参数指定TTF字体文件")
            return 1
        print(f"使用系统字体: {font_path}")
    else:
        if not os.path.exists(font_path):
            print(f"错误: 字体文件 '{font_path}' 不存在")
            return 1
        print(f"使用指定字体: {font_path}")

    # 加载字体
    try:
        font = ImageFont.truetype(font_path, args.size)
    except Exception as e:
        print(f"错误: 无法加载字体文件: {e}")
        return 1

    # 生成字模数据
    print("\n正在生成字模数据...")
    chars_data = []

    for i, char in enumerate(chars):
        print(f"处理字符 '{char}' ({i+1}/{len(chars)})")

        try:
            bitmap, width, height = char_to_bitmap(char, font, args.size)
            hex_data, padded_width = bitmap_to_hex_data(bitmap, width, height)

            chars_data.append({
                'char': char,
                'width': padded_width,
                'height': height,
                'hex_data': hex_data
            })

        except Exception as e:
            print(f"处理字符 '{char}' 时出错: {e}")
            continue

    if not chars_data:
        print("错误: 没有成功生成任何字模数据")
        return 1

    # 确定数组名称
    array_name = args.name
    if not array_name:
        array_name = f"font_{args.size}"

    # 确保输出目录存在
    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # 生成C文件
    print(f"\n正在生成C文件: {args.output}")
    if generate_c_file(chars_data, args.output, array_name):
        print("字模文件生成成功！")

        # 显示统计信息
        print(f"\n生成统计:")
        print(f"  - 成功生成 {len(chars_data)} 个字符的字模")
        print(f"  - 数组名称: {array_name}")
        print(f"  - 字体大小: {args.size}px")

        return 0
    else:
        return 1

if __name__ == "__main__":
    sys.exit(main())
