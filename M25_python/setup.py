from setuptools import setup, find_packages

import m25_plugin.widget.napari_entry_point

PACKAGES = [package for package in find_packages()]

if __name__ == '__main__':
    setup(
        name = 'M25_viewer',
        license = 'Creative Commons Attribution-Noncommercial-Share Alike license',
        packages =PACKAGES,
        include_package_data=True,
        entry_points = {
            'napari.plugin': [
                'M25_viewer = m25_plugin.widget.napari.entry_point',
            ],
        },
        version="0.0.1dev",
    )