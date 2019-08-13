#!/usr/bin/env python
import click
import numpy as np
import tifffile
from .evaluate import evaluate

@click.command()
@click.option('-g', '--groudtruth-path', required=True, 
              type=click.Path(exists=True, dir_okay=False, 
                              resolve_path=True, allow_dash=True), 
              help='groundtruth tif file path.')
@click.option('-s', '--segmentation-path', required=True, 
              type=click.Path(exists=True, dir_okay=False, 
                              resolve_path=True, allow_dash=True), 
              help='segmentation tif file path.')
def cli(groundtruth_path, segmentation_path):
    grt = tifffile.imread(groundtruth_path)
    seg = tifffile.imread(segmentation_path)

    # evalute only works with uint64 data type
    if not np.issubdtype(grt.dtype, np.uint64): 
        grt = grt.astype(np.uint64)

    if not np.issubdtype(seg.dtype, np.uint64):
        seg = seg.astype(np.uint64) 

    evaluate(grt, seg)


if __name__ == '__main__':
    cli()
