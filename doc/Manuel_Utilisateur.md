---
lang: fr-FR
title: Manuel Utilisateur - CmdS
subtitle: Manuel utilisateur du lanceur de commande CmdS
author:
    - Rémi Labbé
date: Janvier 2022
documentclass: report
toc: true
fontsize: 12
mainfont: Source Code Pro Medium
monofont: mononoki Nerd Font
---

# Compilation

- Compilation du serveur
```bash
make cmds
```
- Compilation du client
```bash
make cmdc
```
- Compilation complete
```bash
make
```

# Execution

l'executable `cmds` controle le demon, il accepte 1 argument qui peut prendre 2
 valeurs.

- Pour lancer le demon:
```
./cmds start
```

- Pour arreter le demon:
```
./cmds stop
```

Un client peut etre cree via l'executable `cmdc`, il presentera ensuite un
 "prompt" en attente des requetes. Ce client peut ensuite etre ferme avec Ctrl+D
  ou Ctrl+C.

- pour ouvrir un client et se connecter au serveur:
```
./cmdc
```

Ces differentes informations sont aussi disponibles et affichees si un ou des
 arguments invalides sont presents dans la commande.
