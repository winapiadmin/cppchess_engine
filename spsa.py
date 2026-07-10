import argparse
import csv
import random
import subprocess
import os
import json
import logging
import re
import copy
import tempfile
from collections import defaultdict
logger = logging.getLogger("spsa_tuner")
FASTCHESS_TEMPLATE = {
    "resign": {
        "move_count": 1,
        "score": 0,
        "twosided": False,
        "enabled": False
    },
    "draw": {
        "move_number": 0,
        "move_count": 1,
        "score": 0,
        "enabled": False
    },
    "maxmoves": {
        "move_count": 1,
        "enabled": False
    },
    "tb_adjudication": {
        "syzygy_dirs": "",
        "max_pieces": 0,
        "ignore_50_move_rule": False,
        "enabled": False
    },
    "opening": {
        "file": "UHO_Lichess_4852_v1.epd",
        "format": 0,
        "order": 0,
        "plies": -1,
        "start": 1
    },
    "pgn": {
        "additional_lines_rgx": [],
        "event_name": "Fastchess Tournament",
        "site": "?",
        "file": "",
        "notation": 0,
        "append_file": True,
        "track_nodes": False,
        "track_seldepth": False,
        "track_nps": False,
        "track_hashfull": False,
        "track_tbhits": False,
        "track_timeleft": False,
        "track_latency": False,
        "track_pv": False,
        "min": False,
        "crc": False
    },
    "epd": {
        "file": "",
        "append_file": True
    },
    "sprt": {
        "alpha": 0.05,
        "beta": 0.05,
        "elo0": 0.0,
        "elo1": 5.0,
        "model": "normalized",
        "enabled": True
    },
    "config_name": "config.json",
    "output": 0,
    "variant": 0,
    "type": 0,
    "gauntlet_seeds": 1,
    "seed": 0,
    "ratinginterval": 10,
    "scoreinterval": 1,
    "wait": 0,
    "autosaveinterval": 5,
    "games": 2,
    "rounds": 2000,
    "concurrency": 6,
    "force_concurrency": False,
    "recover": True,
    "noswap": False,
    "reverse": False,
    "report_penta": True,
    "affinity": False,
    "show_latency": False,
    "log": {
        "file": "",
        "level": 2,
        "append_file": True,
        "compress": False,
        "realtime": True,
        "engine_coms": False
    },
    "engines": [
        {
            "name": "Plus",
            "dir": "",
            "cmd": "engine",
            "args": "",
            "restart": False,
            "options": [],
            "limit": {
                "tc": {
                    "increment": 0,
                    "fixed_time": 0,
                    "time": 0,
                    "moves": 0,
                    "timemargin": 0
                },
                "nodes": 0,
                "plies": 5
            },
            "variant": 0
        },
        {
            "name": "Minus",
            "dir": "",
            "cmd": "engine",
            "args": "",
            "restart": False,
            "options": [],
            "limit": {
                "tc": {
                    "increment": 0,
                    "fixed_time": 0,
                    "time": 0,
                    "moves": 0,
                    "timemargin": 0
                },
                "nodes": 0,
                "plies": 5
            },
            "variant": 0
        }
    ],
    "stats": {
        "Plus vs Minus": {
            "wins": 0,
            "losses": 0,
            "draws": 0,
            "penta_WW": 0,
            "penta_WD": 0,
            "penta_WL": 0,
            "penta_DD": 0,
            "penta_LD": 0,
            "penta_LL": 0
        }
    }
}

# --- CLI Parser ---
parser = argparse.ArgumentParser(description="a SPSA tuner")
parser.add_argument("--engine", required=True, help="engine")
parser.add_argument("--infile", default="spsa_params.txt", help="inputs")
parser.add_argument("--outfile", default="spsa_params.txt", help="outs")
parser.add_argument("--iters", type=int, default=10, help="iters")
parser.add_argument("--pairs", type=int, default=4, help="pairs (including repeats)")
parser.add_argument("--workers", type=int, default=6, help="concurrency")
parser.add_argument("--stable_offset", type=int, default=3000, help="stability const")
parser.add_argument("--lr", type=float, default=1e-1, help="base lr")
parser.add_argument("--alpha", type=float, default=0.602, help="alpha")
parser.add_argument("--gamma", type=float, default=0.101, help="gamma")
parser.add_argument("--hash", type=str, default="16", help="TT size")

# --- params ---
def load_params(path, engine_path):
    params = {}
    if os.path.exists(path):
        with open(path) as f:
            for row in csv.reader(f):
                try:
                    if not row:
                        continue
                    line = row[0].strip()

                    if line.startswith("(") and line.endswith(")"):
                        line = line[1:-1]
                    if len(row) == 6:
                        name, val, lo, hi, step, a = row
                        c = max(2.0 * float(step), 1.0)
                    elif len(row) == 7:
                        name, val, lo, hi, step, a, c = row
                    else:
                        raise ValueError(f"Bad row: {row}")
                    params[name] = {
                        "value": float(val),
                        "min": float(lo),
                        "max": float(hi),
                        "step": float(step),
                        "a": float(a),
                        "c": float(c),
                    }
                except Exception as e:print(e, row)
        return params
    else:
        logger.info("%s not found, starting engine to capture parameters", path)
        with tempfile.NamedTemporaryFile(mode="r+", delete=False) as tmp:
            subprocess.run([engine_path], stdout=tmp, stderr=subprocess.STDOUT, input="quit\n", text=True)
            tmp.seek(0)
            return load_params(tmp.name, engine_path)

def save_params(path, params):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        for n, p in params.items():
            w.writerow([n, p["value"], p["min"], p["max"], p["step"], p["a"], p["c"]])

# --- runner ---
def run_fastchess_match(plus_params, minus_params, args, iteration):
    config = copy.deepcopy(FASTCHESS_TEMPLATE)
    config["rounds"] = args.pairs
    config["concurrency"] = args.workers
    config["pgn"]["file"]=f"games{random.randint(0,2**31-1)}.pgn"
    config["epd"]["file"]=f"games{random.randint(0,2**31-1)}.epd"
    options_plus = [["Hash", str(args.hash)]]
    for n, v in plus_params.items():
        options_plus.append([n, str(int(round(v)))])

    options_minus = [["Hash", str(args.hash)]]
    for n, v in minus_params.items():
        options_minus.append([n, str(int(round(v)))])

    config["engines"][0]["cmd"] = args.engine
    config["engines"][0]["options"] = options_plus

    config["engines"][1]["cmd"] = args.engine
    config["engines"][1]["options"] = options_minus

    temp_config_path = "config.json"
    with open(temp_config_path, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=4)

    logger.info(f"--- Iteration {iteration}: {args.pairs} pairs ---")
    cmd = ["./fastchess", "-config", f"file={temp_config_path}"]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        print("Return code:", e.returncode)
        print("STDOUT:")
        print(e.stdout)
        print("STDERR:")
        print(e.stderr)
    with open(temp_config_path, "r", encoding="utf-8") as f:
        output_config = json.load(f)
    print(json.dumps(output_config["stats"], indent=2))
    stats = output_config.get("stats", {})
    stats_key = "Plus vs Minus"

    if stats_key in stats and "wins" in stats[stats_key]:
        w = stats[stats_key]["wins"]
        l = stats[stats_key]["losses"]
        d = stats[stats_key]["draws"]
    else:
        w, l, d = 0, 0, 0
        logger.error("HOW IS THAT NOT EXIST")

    total_games = w + l + d
    if total_games == 0:
        logger.error("as we saw")
        logger.debug(result.stdout)
        logger.debug(result.stderr)
        return 0.5, 0.5

    f_plus = (w + 0.5 * d) / total_games
    f_minus = (l + 0.5 * d) / total_games

    logger.info(f"Plus: WDL=[{w},{d},{l}] total={total_games}")
    return f_plus, f_minus


INDEX_RE = re.compile(r"\[(\d+)\]")
def regroup_ndim(names, values):
    scalars = {}
    arrays_raw = defaultdict(dict)
    for n, v in zip(names, values):
        indices = [int(i) for i in INDEX_RE.findall(n)]
        if indices:
            base_name = n.split('[')[0].strip()
            arrays_raw[base_name][tuple(indices)] = int(round(v))
        else:
            scalars[n] = int(round(v))
    return scalars, arrays_raw

def finalize_ndim_arrays(arrays_raw):
    out_arrays = {}
    for name, coord_dict in arrays_raw.items():
        sample_coords = list(coord_dict.keys())[0]
        ndim = len(sample_coords)
        shape = []
        for d in range(ndim):
            max_idx = max(coords[d] for coords in coord_dict.keys())
            shape.append(max_idx + 1)
                     
        def create_nested_list(dims):
            if len(dims) == 1:
                return [0] * dims[0]
            return [create_nested_list(dims[1:]) for _ in range(dims[0])]
                 
        nested_arr = create_nested_list(shape)
        for coords, val in coord_dict.items():
            current = nested_arr
            for i in range(len(coords) - 1):
                current = current[coords[i]]
            current[coords[-1]] = val
        out_arrays[name] = (shape, nested_arr)
    return out_arrays

def format_cpp_array(arr):
    if not isinstance(arr, list):
        return str(arr)
    inner = ", ".join(format_cpp_array(item) for item in arr)
    return f"{{ {inner} }}"

def write_weights_header(names, x):
    scalars, arrays_raw = regroup_ndim(names, x)
    arrays = finalize_ndim_arrays(arrays_raw) if arrays_raw else {}
    lines = [
        "#ifndef WEIGHTS_H",
        "#define WEIGHTS_H",
        '#include "eval.h"',
        "namespace engine::eval {",
    ]
    for k, v in scalars.items():
        lines.append(f"inline Value {k} = {v};")
    for k, (shape, arr) in arrays.items():
        shape_str = "".join(f"[{dim}]" for dim in shape)
        cpp_initializer = format_cpp_array(arr)
        lines.append(f"inline Value {k}{shape_str} = {cpp_initializer};")
    lines.append("} // namespace engine::eval")
    lines.append("#endif")
    
    # Also write to the default location
    with open("Weights.h", "w", encoding="utf-8") as f:
        f.writelines(l + "\n" for l in lines)
# --- SPSA Core ---
def spsa_core(params, args):
    for k in range(args.iters):
        logger.info(f"=== iter: {k} ===")

        deltas = {n: (1 if random.random() < 0.5 else -1) for n in params}
        ak = {n: p["a"] / (k + args.stable_offset) ** args.alpha for n, p in params.items()}
        ck = {
            n: p["c"] / (k + 1) ** args.gamma
            for n, p in params.items()
        }

        while True:

            deltas = {
                n: 1 if random.random() < 0.5 else -1
                for n in params
            }

            plus = {
                n: min(
                    p["max"],
                    max(
                        p["min"],
                        p["value"] + ck[n] * deltas[n]
                    ),
                )
                for n, p in params.items()
            }

            minus = {
                n: min(
                    p["max"],
                    max(
                        p["min"],
                        p["value"] - ck[n] * deltas[n]
                    ),
                )
                for n, p in params.items()
            }

            plus_int = {
                n: int(round(v))
                for n, v in plus.items()
            }

            minus_int = {
                n: int(round(v))
                for n, v in minus.items()
            }

            if plus_int != minus_int:
                break
        plus_int = {n: int(round(v)) for n, v in plus.items()}
        minus_int = {n: int(round(v)) for n, v in minus.items()}

        if plus_int == minus_int:
            continue
        f_plus, f_minus = run_fastchess_match(plus, minus, args, k)
        
        for n, p in params.items():
            score = 2.0 * f_plus - 1.0

            ghat = score / (
                2 * ck[n] * deltas[n]
            )
            old_val = p["value"]
            new_val = old_val + ak[n] * ghat
            p["value"] = min(p["max"], max(p["min"], new_val))
            
            if int(round(old_val)) != int(round(p["value"])):
                logger.info(
                    "[%s] %.3f -> %.3f | grad=% .4f ak=%.4f ck=%.3f",
                    n,
                    old_val,
                    p["value"],
                    ghat,
                    ak[n],
                    ck[n],
                )

        print(f"iter {k}: score (plus) = {f_plus:.3f}")
        save_params(args.outfile, params)
        write_weights_header(list(params.keys()), [p["value"] for p in params.values()])

    return params

def main():
    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%H:%M:%S"
    )
    args = parser.parse_args()
    
    logger.info("loading params")
    params = load_params(args.infile, args.engine)
    for name, p in params.items():

        assert p["min"] <= p["value"] <= p["max"]

        assert p["step"] > 0
        assert p["a"] > 0
        assert p["c"] > 0
    write_weights_header(list(params.keys()), [p["value"] for p in params.values()])
    logger.info(f"got {len(params)} params.")
    
    tuned_params = spsa_core(params, args)
    save_params(args.outfile, tuned_params)
    logger.info("FINISHED. now run a SPRT.")

if __name__ == "__main__":
    main()
