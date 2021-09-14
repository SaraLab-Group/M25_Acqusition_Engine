from setuptools import setup, find_packages

LICENSE = 'Creative Commons Attribution-Noncommercial-Share Alike license'
DISTNAME = 'M25_visualization'
DESCRIPTION = 'Tool for visualizing and acquiring Multifocus Microscopy Data'
PACKAGES = [package for package in find_packages()]
CLASSIFIERS =[
            # How mature is this project? Common values are
            #   3 - Alpha
            #   4 - Beta
            #   5 - Production/Stable
            'Development Status :: 3 - Alpha',

            # Indicate who your project is intended for
            'Intended Audience :: Developers',
            'Topic :: Software Development :: Build Tools',

            # Pick your license as you wish
            'License :: OSI Approved :: MIT License',

            # Specify the Python versions you support here. In particular, ensure
            # that you indicate you support Python 3. These classifiers are *not*
            # checked by 'pip install'. See instead 'python_requires' below.
            'Programming Language :: Python :: 3',
            'Programming Language :: Python :: 3.6',
            'Programming Language :: Python :: 3.7',
            'Programming Language :: Python :: 3.8',
            'Programming Language :: Python :: 3.9',
            'Programming Language :: Python :: 3 :: Only'
        ]
use_scm = {"write_to": "m25_controls/_version.py"}

if __name__ == '__main__':
    setup(
        name = DISTNAME,
        description = DESCRIPTION,
        license = LICENSE,
        packages =PACKAGES,
        include_package_data=True,
        classifiers = CLASSIFIERS,
        keywords='multifocus microscopy, microscopy, fluorescence',  # Optional
        entry_points = {
            'napari.plugin': [
                'M25_viewer = m25_controls',
            ],
        },
        python_requires='>=3.6, <4',
        # use_scm_version = use_scm,
        version="0.0.1dev",
    )