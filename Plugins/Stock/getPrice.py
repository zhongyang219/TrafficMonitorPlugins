import mmap
import ctypes
from pytdx.hq import TdxHq_API
import time
import win32event
import configparser
import os
import requests
from datetime import datetime

MAX_WATCH_NUM = 32
# 双缓冲事件定义
EVENT_NAME_A = "Local\\TdxQuoteEventA"
EVENT_NAME_B = "Local\\TdxQuoteEventB"
# 双缓冲共享内存名
SHARE_NAME = "Local\\TdxQuoteShareDoubleBuf"

class QuoteItem(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("code", ctypes.c_char * 16),
        ("name", ctypes.c_char * 32),
        ("price", ctypes.c_double),
        ("open", ctypes.c_double),
        ("high", ctypes.c_double),
        ("low", ctypes.c_double),
        ("pre_close", ctypes.c_double),
        ("bid1", ctypes.c_double),
        ("bid_vol1", ctypes.c_longlong),
        ("bid2", ctypes.c_double),
        ("bid_vol2", ctypes.c_longlong),
        ("bid3", ctypes.c_double),
        ("bid_vol3", ctypes.c_longlong),
        ("bid4", ctypes.c_double),
        ("bid_vol4", ctypes.c_longlong),
        ("bid5", ctypes.c_double),
        ("bid_vol5", ctypes.c_longlong),
        ("ask1", ctypes.c_double),
        ("ask_vol1", ctypes.c_longlong),
        ("ask2", ctypes.c_double),
        ("ask_vol2", ctypes.c_longlong),
        ("ask3", ctypes.c_double),
        ("ask_vol3", ctypes.c_longlong),
        ("ask4", ctypes.c_double),
        ("ask_vol4", ctypes.c_longlong),
        ("ask5", ctypes.c_double),
        ("ask_vol5", ctypes.c_longlong),
        ("vol", ctypes.c_longlong),
        ("amount", ctypes.c_double),
        ("inner_vol", ctypes.c_longlong),
        ("outer_vol", ctypes.c_longlong),
        ("cur_vol", ctypes.c_longlong),
    ]

class ShareMemHeader(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("seq", ctypes.c_uint),
        ("item_count", ctypes.c_int),
        ("items", QuoteItem * MAX_WATCH_NUM)
    ]

SERVER_LIST = [
    ("114.28.173.139", 7709),
    ("119.147.172.134", 7709),
    ("113.105.142.116", 7709),
    ("47.108.174.138", 7709),
    ("103.196.128.133", 7709),
    ("218.6.176.103", 7709),
    ("124.71.180.111", 7709),
    ("112.74.187.171", 7709),
    ("120.92.206.146", 7709),
    ("182.92.87.171", 7709),
    ("114.80.63.171", 7709),
]
REFRESH_INTERVAL_TRADE = 1.0
REFRESH_INTERVAL_IDLE = 10.0

api = TdxHq_API()
current_server_idx = 0
single_buf_size = ctypes.sizeof(ShareMemHeader)
# 双缓冲总大小：两块结构体
double_buf_size = single_buf_size * 2
# 创建双缓冲共享内存
mm = mmap.mmap(-1, double_buf_size, SHARE_NAME)

# 创建两个自动重置事件
h_event_a = win32event.CreateEvent(None, False, False, EVENT_NAME_A)
h_event_b = win32event.CreateEvent(None, False, False, EVENT_NAME_B)

seq = 0
quote_cache = {}
code2name = {}
low_data_count = 0
LOW_DATA_THRESHOLD = 2
MIN_VALID_COUNT = 3
conn_start_time = time.time()
MAX_CONN_SECONDS = 300
name_cache_loaded = False

# 双缓冲标记：0=空闲写A，1=空闲写B
active_write_buf = 0
# 上一轮完整二进制快照，用于对比数据是否变化
last_buf_snapshot = b""

def is_trade_time() -> bool:
    now = datetime.now()
    total_min = now.hour * 60 + now.minute
    morning = 9*60+30 <= total_min < 11*60+30
    afternoon = 13*60 <= total_min < 15*60
    return morning or afternoon

def load_watch_list_from_ini():
    import sys
    if hasattr(sys, '_MEIPASS'):
        exe_full_path = sys.executable
        app_path = os.path.dirname(os.path.abspath(exe_full_path))
    else:
        app_path = os.path.dirname(os.path.abspath(__file__))
    ini_path = os.path.join(app_path, "Stock.ini")
    if not os.path.exists(ini_path):
        print(f"❌找不到配置文件：{ini_path}")
        raise FileNotFoundError("Stock.ini不存在，请放置在exe同目录")
    cfg = configparser.ConfigParser()
    with open(ini_path, "r", encoding="utf-8-sig") as fp:
        content = fp.read()
        cfg.read_string(content)
    raw_str = cfg.get("config", "stock_code")
    raw_items = [s.strip().replace('"', '') for s in raw_str.split(",")]
    watch = []
    for item in raw_items:
        if not item:
            continue
        if item.startswith("sh"):
            market = 1
            pure_code = item[2:]
            watch.append((market, pure_code, item))
        elif item.startswith("sz"):
            market = 0
            pure_code = item[2:]
            watch.append((market, pure_code, item))
        elif item.startswith("rt_hk"):
            print(f"⚠️跳过港股标的 {item}")
        else:
            print(f"⚠️无法识别市场前缀，跳过:{item}")
    print(f"\n✅成功加载沪深标的总数：{len(watch)}")
    for m, c, raw in watch:
        print(f"   market={m}, pure={c}, raw={raw}")
    return watch

def load_stock_name_cache(watch):
    global code2name, name_cache_loaded
    if name_cache_loaded:
        print("ℹ️股票名称缓存已加载，跳过新浪接口请求")
        return
    code2name.clear()
    print("\n🔍首次加载股票名称信息（新浪接口）...")
    req_list = []
    map_prefix = {}
    for market, pure_code, raw_code in watch:
        prefix = "sz" if market == 0 else "sh"
        key = f"{prefix}{pure_code}"
        req_list.append(key)
        map_prefix[pure_code] = key
    try:
        url = f"http://hq.sinajs.cn/list={','.join(req_list)}"
        headers = {"Referer": "https://finance.sina.com.cn/"}
        resp = requests.get(url, headers=headers, timeout=8)
        resp.encoding = "gbk"
        text = resp.text
        name_map = {}
        lines = text.strip().splitlines()
        for line in lines:
            if "hq_str_" not in line or '="' not in line:
                continue
            code_key = line.split("=")[0].replace("var hq_str_", "")
            data_part = line.split('"')[1]
            fields = data_part.split(",")
            stock_name = fields[0]
            name_map[code_key] = stock_name
        success_cnt = 0
        fail_cnt = 0
        for market, pure_code, raw_code in watch:
            pre_key = map_prefix[pure_code]
            if pre_key in name_map and name_map[pre_key].strip():
                code2name[pure_code] = name_map[pre_key]
                success_cnt += 1
                print(f"✅ {pure_code} → {code2name[pure_code]}")
            else:
                code2name[pure_code] = pure_code
                fail_cnt += 1
                print(f"❌ {pure_code} 获取名称失败")
        print(f"📋名称加载完成：成功{success_cnt}个，失败{fail_cnt}\n")
    except Exception as e:
        print(f"⚠️新浪接口请求异常：{e}，全部标的名称使用代码兜底！")
        for market, pure_code, raw_code in watch:
            code2name[pure_code] = pure_code
    name_cache_loaded = True

def try_connect_next_server(watch):
    global current_server_idx, conn_start_time
    api.close()
    try_times = 0
    while try_times < len(SERVER_LIST):
        host, port = SERVER_LIST[current_server_idx]
        print(f"\n尝试连接行情服务器：{host}:{port}")
        try:
            ret = api.connect(host, port)
            if ret:
                print(f"✅ 连接成功 {host}:{port}")
                conn_start_time = time.time()
                load_stock_name_cache(watch)
                return True
        except Exception as e:
            print(f"❌连接失败 {host}:{port} , err:{e}")
        current_server_idx = (current_server_idx + 1) % len(SERVER_LIST)
        try_times += 1
        time.sleep(0.3)
    print("❌所有服务器全部连接失败，等待10s后重试")
    time.sleep(10)
    return False

watch_list = load_watch_list_from_ini()
if len(watch_list) == 0:
    print("❌没有有效的沪深标的，程序退出")
    exit(1)
try_connect_next_server(watch_list)

while True:
    try:
        now = time.time()
        if now - conn_start_time > MAX_CONN_SECONDS:
            print(f"\n⏱连接已超过{MAX_CONN_SECONDS}秒，主动重建连接刷新通道")
            quote_cache.clear()
            low_data_count = 0
            api.close()
            time.sleep(0.5)
            try_connect_next_server(watch_list)
            continue

        temp_result_map = {}
        success_codes = []
        fail_codes = []
        query_list = [(m, pc) for m, pc, raw in watch_list]
        ret_list = api.get_security_quotes(query_list)
        if ret_list and len(ret_list) > 0:
            for data in ret_list:
                pure_code = data["code"]
                quote_cache[pure_code] = data
                temp_result_map[pure_code] = data
                success_codes.append(pure_code)
        for market, pure_code, raw_code in watch_list:
            if pure_code not in temp_result_map:
                fail_codes.append(pure_code)
                if pure_code in quote_cache:
                    temp_result_map[pure_code] = quote_cache[pure_code]
        print(f"本次成功联网获取标的:{len(success_codes)} 联网失败:{len(fail_codes)}")
        if len(fail_codes) > 0:
            print(f"联网查询失败代码列表：{fail_codes}")

        all_data = []
        for market, pure_code, raw_code in watch_list:
            if pure_code in temp_result_map:
                all_data.append((raw_code, temp_result_map[pure_code]))

        if len(success_codes) < MIN_VALID_COUNT:
            low_data_count += 1
            print(f"⚠️有效返回标的过少，累计异常轮次：{low_data_count}/{LOW_DATA_THRESHOLD}")
        else:
            low_data_count = 0
        if low_data_count >= LOW_DATA_THRESHOLD:
            print("🚨连续2轮获取标的数量不足，主动切换服务器！")
            low_data_count = 0
            quote_cache.clear()
            api.close()
            time.sleep(0.5)
            try_connect_next_server(watch_list)
            time.sleep(REFRESH_INTERVAL_TRADE)
            continue
        if len(all_data) == 0:
            print("⚠️行情列表全部为空，无任何缓存数据")
            sleep_sec = REFRESH_INTERVAL_TRADE if is_trade_time() else REFRESH_INTERVAL_IDLE
            time.sleep(sleep_sec)
            continue

        # 组装当前完整行情结构体
        shmem = ShareMemHeader()
        ctypes.memset(ctypes.addressof(shmem), 0, single_buf_size)
        shmem.item_count = len(all_data)
        for idx, (raw_code, d) in enumerate(all_data):
            item = QuoteItem()
            item.code = raw_code.encode("utf8")
            pure_code = d["code"]
            stock_name = code2name.get(pure_code, pure_code)
            item.name = stock_name.encode("gbk", errors="ignore")
            item.price = d["price"]
            item.open = d["open"]
            item.high = d["high"]
            item.low = d["low"]
            item.pre_close = d["last_close"]
            item.bid1 = d["bid1"]
            item.bid_vol1 = d["bid_vol1"]
            item.bid2 = d["bid2"]
            item.bid_vol2 = d["bid_vol2"]
            item.bid3 = d["bid3"]
            item.bid_vol3 = d["bid_vol3"]
            item.bid4 = d["bid4"]
            item.bid_vol4 = d["bid_vol4"]
            item.bid5 = d["bid5"]
            item.bid_vol5 = d["bid_vol5"]
            item.ask1 = d["ask1"]
            item.ask_vol1 = d["ask_vol1"]
            item.ask2 = d["ask2"]
            item.ask_vol2 = d["ask_vol2"]
            item.ask3 = d["ask3"]
            item.ask_vol3 = d["ask_vol3"]
            item.ask4 = d["ask4"]
            item.ask_vol4 = d["ask_vol4"]
            item.ask5 = d["ask5"]
            item.ask_vol5 = d["ask_vol5"]
            item.vol = d["vol"]
            item.amount = d["amount"]
            item.inner_vol = d["s_vol"]
            item.outer_vol = d["b_vol"]
            item.cur_vol = d["cur_vol"]
            shmem.items[idx] = item
        seq += 1
        shmem.seq = seq
        # 转二进制快照，对比上一轮数据是否完全一致
        current_bin = ctypes.string_at(ctypes.addressof(shmem), single_buf_size)
        if current_bin == last_buf_snapshot:
            print("ℹ️行情数据无变化，跳过写入共享内存")
            sleep_sec = REFRESH_INTERVAL_TRADE if is_trade_time() else REFRESH_INTERVAL_IDLE
            time.sleep(sleep_sec)
            continue
        # 更新快照
        last_buf_snapshot = current_bin

        # 双缓冲写入逻辑
        buf_offset = active_write_buf * single_buf_size
        mm.seek(buf_offset)
        # 先清空当前缓冲区整块0
        mm.write(b'\x00' * single_buf_size)
        # 写入新数据
        mm.write(current_bin)
        # 触发对应事件
        if active_write_buf == 0:
            win32event.SetEvent(h_event_a)
            print(f"✅写入缓冲A，发送EventA通知MFC，seq={seq}")
        else:
            win32event.SetEvent(h_event_b)
            print(f"✅写入缓冲B，发送EventB通知MFC，seq={seq}")
        # 切换下一次写入的空闲缓冲
        active_write_buf = 1 - active_write_buf

        # 调试打印
        for idx in range(shmem.item_count):
            it = shmem.items[idx]
            print(f"{it.code.decode()},{it.price},{it.bid_vol1}")
        print()

    except Exception as e:
        print("❌查询异常，准备切换服务器:", e)
        import traceback
        traceback.print_exc()
        quote_cache.clear()
        low_data_count = 0
        api.close()
        time.sleep(0.5)
        try_connect_next_server(watch_list)

    # 分时休眠
    sleep_sec = REFRESH_INTERVAL_TRADE if is_trade_time() else REFRESH_INTERVAL_IDLE
    time.sleep(sleep_sec)