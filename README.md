pinentry-dmenu
==============

pinentry-dmenu is a pinentry program with the charm of [dmenu](https://tools.suckless.org/dmenu).

This program is a fork from [spine](https://gitgud.io/zavok/spine.git) which is also a fork from [dmenu](https://tools.suckless.org/dmenu).


NO FURTHER DEVELOPMENT
----------------------

This project is no longer under development. If you have another opinion feel free to fork it.


Requirements
------------
In order to build dmenu you need the Xlib header files.


Installation
------------
Edit config.mk to match your local setup (dmenu is installed into the /usr/local namespace by default).

Afterwards enter the following command to build and install dmenu
(if necessary as root):

	make clean install


Config
------
To use pinentry-dmenu add in `~/.gnupg/gpg-agent.conf`:

	pinentry-program <absolut path to pinentry-dmenu>

The config is located in `~/.gnupg/pinentry-dmenu.conf`.

Parameter           | Default           | Description
:------------------ |:----------------- |:-----------
asterisk            | *                 | Defines the symbol which is showed for each typed character
bottom              | false             | pinentry-dmenu appears at the bottom of the screen
min_password_length | 32                | The minimal space of the password field. This value has affect to the description field after the password field
monitor             | -1                | pinentry-dmenu is displayed on the monitor number supplied. Monitor numbers are starting from 0
prompt              | ""                | Defines the prompt to be displayed to the left of the input field
font                | monospace:size=10 | Defines the font or font set used
prompt_bg           | #bbbbbb           | Defines the prompt background color
prompt_fg           | #222222           | Defines the prompt foreground color
normal_bg           | #bbbbbb           | Defines the normal background color
normal_fg           | #222222           | Defines the normal foreground color
select_bg           | #eeeeee           | Defines the selected background color
select_fg           | #005577           | Defines the selected foreground color
desc_bg             | #bbbbbb           | Defines the description background color
desc_fg             | #222222           | Defines the description foreground color
embedded            | false             | Embed into window


Example
-------
```
asterisk= "# ";
prompt = "$";
font = "Noto Sans UI:size=13";
prompt_fg = "#eeeeee";
prompt_bg = "#d9904a";
normal_fg = "#ffffff";
normal_bg = "#000000";
select_fg = "#eeeeee";
select_bg = "#d9904a";
desc_fg = "#eeeeee";
desc_bg = "#d9904a";
```
