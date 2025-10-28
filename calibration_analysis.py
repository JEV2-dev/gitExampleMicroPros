import argparse, os, json
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

def adjusted_r2(r2, n, k):
    """Adjusted R^2 para comparar lineal vs polinómico (k = # predictores)."""
    return 1 - (1 - r2) * (n - 1) / (n - k - 1) if n > k + 1 else float("nan")

def fit_and_report(x, y, x_label, y_label, out_prefix):
    """
    Ajusta modelos lineal y cuadrático, elige el mejor por adjusted R^2,
    guarda el gráfico, y devuelve resultados.
    """
    results = {}
    n = len(x)

    # Lineal: y = a*x + b
    a1, b1 = np.polyfit(x, y, 1)
    y1 = a1 * x + b1
    ss_res1 = np.sum((y - y1) ** 2)
    ss_tot = np.sum((y - np.mean(y)) ** 2)
    r2_1 = 1 - ss_res1 / ss_tot if ss_tot != 0 else float("nan")
    ar2_1 = adjusted_r2(r2_1, n, 1)
    results["linear"] = {"coeffs": [a1, b1], "r2": r2_1, "adj_r2": ar2_1}

    # Cuadrático: y = c*x^2 + a*x + b
    c2, a2, b2 = np.polyfit(x, y, 2)
    y2 = c2 * x**2 + a2 * x + b2
    ss_res2 = np.sum((y - y2) ** 2)
    r2_2 = 1 - ss_res2 / ss_tot if ss_tot != 0 else float("nan")
    ar2_2 = adjusted_r2(r2_2, n, 2)
    results["quadratic"] = {"coeffs": [c2, a2, b2], "r2": r2_2, "adj_r2": ar2_2}

    # Elegir mejor por adjusted R^2
    best_kind = max(results, key=lambda k: results[k]["adj_r2"] if not np.isnan(results[k]["adj_r2"]) else -1e9)
    best = results[best_kind]

    # Graficar datos + mejor ajuste
    xs = np.linspace(x.min(), x.max(), 300)
    plt.figure()
    plt.scatter(x, y, s=12)
    if best_kind == "linear":
        a, b = best["coeffs"]
        plt.plot(xs, a*xs + b)
        eq = f"{y_label} = {a:.6f}*{x_label} + {b:.6f}"
    else:
        c, a, b = best["coeffs"]
        plt.plot(xs, c*xs**2 + a*xs + b)
        eq = f"{y_label} = {c:.6f}*{x_label}^2 + {a:.6f}*{x_label} + {b:.6f}"

    plt.xlabel(x_label)
    plt.ylabel(y_label)
    plt.title(f"{y_label} vs {x_label}\nBest: {best_kind} (adj R^2={best['adj_r2']:.4f})")
    plot_path = f"{out_prefix}_{best_kind}.png"
    plt.savefig(plot_path, dpi=160, bbox_inches="tight")
    plt.close()

    best["kind"] = best_kind
    best["equation"] = eq
    best["plot_path"] = plot_path
    return results, best

def main():
    ap = argparse.ArgumentParser(description="Calibration analysis: visualización, regresión y gráficos.")
    ap.add_argument("--input", required=True, help="Ruta al CSV. Columnas esperadas: 'ADC Raw Value','Cali Voltage (mV)','Voltage (V)','Sand %','Water %'")
    ap.add_argument("--outdir", default=None, help="Carpeta de salida (por defecto se crea junto al CSV)")
    args = ap.parse_args()

    csv_path = args.input
    # Carpeta de salida por defecto: ./calibration_outputs junto al CSV
    outdir = args.outdir or os.path.join(os.path.dirname(csv_path) or ".", "calibration_outputs")
    os.makedirs(outdir, exist_ok=True)

    # Leer CSV
    df = pd.read_csv(csv_path)
    df.columns = [c.strip() for c in df.columns]

    # Forzar numéricos si existen
    for col in ["ADC Raw Value", "Cali Voltage (mV)", "Voltage (V)", "Sand %", "Water %"]:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    # Si no hay 'Voltage (V)', derivar de 'Cali Voltage (mV)'
    if "Voltage (V)" not in df.columns and "Cali Voltage (mV)" in df.columns:
        df["Voltage (V)"] = df["Cali Voltage (mV)"] / 1000.0

    artifacts = {"input": csv_path, "outdir": outdir, "figures": {}}

    # (1) Curva eléctrica base: Voltage vs Raw
    if "ADC Raw Value" in df.columns and "Voltage (V)" in df.columns:
        mask = df["ADC Raw Value"].notna() & df["Voltage (V)"].notna()
        x = df.loc[mask, "ADC Raw Value"].values
        y = df.loc[mask, "Voltage (V)"].values
        if len(x) >= 3:
            res_all, best = fit_and_report(x, y, "Raw", "Voltage (V)", os.path.join(outdir, "voltage_vs_raw"))
            artifacts["figures"]["voltage_vs_raw"] = {"all_models": res_all, "best": best}
        else:
            artifacts["figures"]["voltage_vs_raw"] = {"error": "No hay suficientes puntos (>=3)"}
    else:
        artifacts["figures"]["voltage_vs_raw"] = {"error": "Faltan columnas requeridas"}

    # (2) Si hay Water %, ajusta Water % vs Voltage (curva de humedad)
    if "Water %" in df.columns and df["Water %"].notna().any() and "Voltage (V)" in df.columns:
        mask = df["Water %"].notna() & df["Voltage (V)"].notna()
        x = df.loc[mask, "Voltage (V)"].values
        y = df.loc[mask, "Water %"].values
        if len(x) >= 3:
            res_all, best = fit_and_report(x, y, "Voltage (V)", "Water %", os.path.join(outdir, "water_vs_voltage"))
            artifacts["figures"]["water_vs_voltage"] = {"all_models": res_all, "best": best}
        else:
            artifacts["figures"]["water_vs_voltage"] = {"error": "No hay suficientes puntos con Water % (>=3)"}
    else:
        artifacts["figures"]["water_vs_voltage"] = {"info": "No se proporcionó 'Water %' o está vacío; se omite."}

    # (3) Si hay Sand %, ajusta Sand % vs Voltage (por si etiquetaste con arena/humedad inversa)
    if "Sand %" in df.columns and df["Sand %"].notna().any() and "Voltage (V)" in df.columns:
        mask = df["Sand %"].notna() & df["Voltage (V)"].notna()
        x = df.loc[mask, "Voltage (V)"].values
        y = df.loc[mask, "Sand %"].values
        if len(x) >= 3:
            res_all, best = fit_and_report(x, y, "Voltage (V)", "Sand %", os.path.join(outdir, "sand_vs_voltage"))
            artifacts["figures"]["sand_vs_voltage"] = {"all_models": res_all, "best": best}
        else:
            artifacts["figures"]["sand_vs_voltage"] = {"error": "No hay suficientes puntos con Sand % (>=3)"}
    else:
        artifacts["figures"]["sand_vs_voltage"] = {"info": "No se proporcionó 'Sand %' o está vacío; se omite."}

    # Guardar reporte de texto con ecuaciones y métricas
    report_txt = os.path.join(outdir, "calibration_report.txt")
    with open(report_txt, "w") as f:
        f.write("Calibration Analysis Report\n")
        f.write(f"Input CSV: {csv_path}\n")
        for name, item in artifacts["figures"].items():
            f.write("\n--- " + name + " ---\n")
            if "best" in item:
                best = item["best"]
                f.write(f"Best model: {best['kind']}\n")
                f.write(f"Equation: {best['equation']}\n")
                f.write(f"Adjusted R^2: {best['adj_r2']:.6f}\n")
                f.write(f"Plot: {best['plot_path']}\n")
            else:
                f.write(json.dumps(item) + "\n")

    # Guardar JSON con modelos para usar luego (ej. generar C)
    models_json = os.path.join(outdir, "models.json")
    with open(models_json, "w") as jf:
        json.dump(artifacts, jf, indent=2)

    print("Done. Outputs at:", outdir)
    print("Report:", report_txt)
    for fig_name, info in artifacts["figures"].items():
        if "best" in info:
            print(f"{fig_name} plot:", info["best"]["plot_path"])

if __name__ == "__main__":
    main()
