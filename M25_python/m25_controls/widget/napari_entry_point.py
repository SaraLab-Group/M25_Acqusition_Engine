from napari_plugin_engine import napari_hook_implementation
from m25_controls.widget.m25_control_widget import M25_widget

"""
Used qt designer to generate the UI

run the command:pyuic5 -x <.ui input file> -o <.py output file>


"""

@napari_hook_implementation
def napari_experimental_provide_dock_widget():
    # you can return either a single widget, or a sequence of widgets
    # each widget is accessible as a new plugin that stacks in the side panel
    return [(M25_widget, {'name': 'M25_viewer'})]
