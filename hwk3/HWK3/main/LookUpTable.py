from sympy import symbols, lambdify

LUT_SIZE = 4096              
x = symbols('x')

# ---- Polinomio HWK2 ---------------------------
intercept = -11.288519230192662
a1 = 1.48860811e-02
a2 = 3.87457794e-06
poly_expr = intercept + a1*x + a2*(x**2)
# ------------------------------------------------

to_int = lambda v: int(round(v))
f = lambdify(x, poly_expr, "math")
lookup = [to_int(f(i)) for i in range(LUT_SIZE)]

header_name = "lookuptable.h"
guard = "LOOKUPTABLE_H"

with open(header_name, "w", encoding="utf-8") as h:
    h.write(f"#ifndef {guard}\n#define {guard}\n\n")
    h.write("#include <stdint.h>\n\n")
    h.write(f"#define LUT_SIZE {LUT_SIZE}\n\n")
    h.write("static const int32_t lookup_table[LUT_SIZE] = {\n")

    per_line = 8
    for i, val in enumerate(lookup):
        end = "\n" if (i + 1) % per_line == 0 else " "
        h.write(f"{val},{end}")
    if LUT_SIZE % per_line != 0:
        h.write("\n")
        h.write("};\n\n")

    h.write(
        "};"
        "static inline int32_t lut_get(int idx) {\n"
        "    if (idx < 0) idx = 0;\n"
        "    if (idx >= LUT_SIZE) idx = LUT_SIZE - 1;\n"
        "    return lookup_table[idx];\n"
        "}\n\n"
    )
    h.write(f"#endif // {guard}\n")

print(f"Generado {header_name} con {LUT_SIZE} entradas.")