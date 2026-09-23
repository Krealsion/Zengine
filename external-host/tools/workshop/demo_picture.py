# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (c) 2026 Joshua DeMoss
from workshop_steps import picture


def run(ctx):
    picture(ctx, ctx.inputs["link"], "demo")
    return "Captured the current demo without injecting input."
