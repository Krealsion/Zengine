# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
from workshop_steps import picture


def run(ctx):
    picture(ctx, ctx.inputs["link"], "demo")
    return "Captured the current demo without injecting input."
