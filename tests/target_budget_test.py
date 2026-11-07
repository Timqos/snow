# SPDX-License-Identifier: GPL-3.0-only
"""A zero-work baseline must reject added loop work without dividing by zero."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
spec=importlib.util.spec_from_file_location('budget',Path(__file__).with_name('target_budget.py'))
budget=importlib.util.module_from_spec(spec);spec.loader.exec_module(budget)
with tempfile.TemporaryDirectory() as directory:
    root=Path(directory)
    dis=root/'test.dis';limits=root/'limits.txt'
    dis.write_text('analog_render:\n 2000000: 00 00  r0 = r0 + 1\n 2000002: 00 00  goto -2 <analog_render : 2000000 >\n')
    limits.write_text('analog_render 0\n')
    budget.dis=str(dis);budget.budget=str(limits);budget.FUNCS=['analog_render']
    result=io.StringIO()
    with contextlib.redirect_stdout(result):assert budget.main()==1
    assert 'OVER BUDGET (new loop work' in result.getvalue()
    limits.write_text('analog_render 2\n')
    with contextlib.redirect_stdout(io.StringIO()):assert budget.main()==0
print('Target budget regression passed: zero-work limits reject growth; positive limits still pass equal work.')
