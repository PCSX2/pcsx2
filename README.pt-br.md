
<div align="center">

![ARMSX2](app_icons/icon.png)

# ARMSX2

[![Licensa](https://img.shields.io/github/license/ARMSX2/ARMSX2)](https://www.gnu.org/licenses/gpl-3.0.html)
[![Discord](https://img.shields.io/discord/914421153827794975?logo=discord&logoColor=white&label=ARMSX2%20Discord&color=5865F2)](https://discord.gg/6yyawTtCnX)
(https://patreon.com/ARMSX2)
[![Versões Nightly ](https://github.com/ARMSX2/ARMSX2/actions/workflows/android_nightly_build.yml/badge.svg)](https://github.com/ARMSX2/ARMSX2/actions/workflows/android_nightly_build.yml)

</div>

ARMSX2 é emulador gratuito e de código aberto de PlayStation 2 (PS2) para dispositivos ARM baseado no PCSX2 e PCSX2_ARM64. O proprósito é emular o hardware do PS2 para dispositivos ARM, usando um recompilador que opera de x86 pra arm64, não arm64 nativo, isso está sujeito a alterações assim que o desenvolvimento continuar. ARMSX2 permite você jogar jogos de PS2 no seu dispositivo móvel android, assim como iOS, Linux, e dispositivos Windows.

## Detalhes do Projeto

ARMSX2 começou após anos em falta de um emulador de código aberto para os sistemas ARM, e então o desenvolvedor [@MoonPower](https://github.com/momo-AUX1) com o suporte de [@jpolo1224](https://github.com/jpolo1224) decidiu tentar fazer um port de um novo emulador de PS2 para Android, fazendo um fork do repositório PCSX2_ARM64 do desenvolvedor Pontos. Moon tem feito e continuará fazendo o seu melhor para preencher as lacunas e fazer ARMSX2 um emulador completo, com o objetivo de manter uma versão de pariedade com a do PCSX2. Esse projeto não é oficialmente associado com PCSX2, e nós não somos associados com nenhum outro fork feito a partir do repositório original. Isso é a nossa própria tentativa de continuar a emulação de PS2 para Android, iOS, e MacOS. O emulador atualmente opera de x86 para arm64, não nativamente arm64, então muito provavelmente a performance não vai ser tão boa como AetherSX2 no momento, porém as coisas estão sujeitas a mudanças ao decorrer do desenvolvimento. 

## Requisitos do Sistema

ARMSX2 suporta qualquer dispositivo ARM capaz, incluindo as plataformas Android, iOS, Linux, e Windows (futuramente, também deve funcionar). Por gentileza, saiba que a performance também irá depender das capacidades de hardware do seu dispositivo, Temos feito o nosso melhor pra optimizar para dispositivos low end e vamos continuar fazendo isso.

Por gentileza saiba que uma imagem da BIOS vinda do seu console PS2 legítimo é necessária para usar o emulador. 

## Website

→ <https://armsx2.net/>

Qualquer outro website não é afiliado com ARMSX2. 

## Traduções

[Ajude traduzir o ARMSX2](https://crowdin.com/project/armsx2-translations/invite?h=940eaf6355b31b5fdb1771183c694ca32710218)

## Baixar

ARMSX2 está disponível pela [Google Play Store](https://play.google.com/store/apps/details?id=come.nanodata.armsx2)

[<img src="https://play.google.com/intl/en_us/badges/static/images/badges/en_badge_web_generic.png" alt="Baixe pela Google Play" height="80"/>](https://play.google.com/store/apps/details?id=come.nanodata.armsx2)

## Afiliação

Nós NÃO somos afiliados de forma/maneira nenhuma com ARM Holding LTD. Nós escolhemos o nome ARMSX2 já que executa em dispositivos ARM, e não queremos nenhum incentivo comercial com o emulador. O máximo que aceitamos é doações voluntárias. Obrigado.

## Créditos Adicionais

[PCSX2](https://github.com/PCSX2/pcsx2) - ARMSX2 não seria possível sem o trabalho, paciência e compreensão lendários da equipe do PCSX2 com relação ao projeto.

[PCSX2_ARM64](https://github.com/pontos2024/PCSX2_ARM64) - ARMSX2 originalmente começou como um fork do trabalho de pontos. 

Obrigado [@Vivimagic](https://github.com/Vivimagic) por criar e trabalhar na logo!

Obrigado aos desenvolvedores [@tanosshi](https://github.com/tanosshi) [@jpolo1224](https://github.com/jpolo1224) [@MoonPower](https://github.com/momo-AUX1) por trabalhar no website do ARMSX2!

## Roadmap

Aqui está o roadmap de coisas que você pode esperar do ARMSX2 no futuro:

| Tarefa | Prioridade |
| --- | --- |
| Arrumar GPUs xclipse | Alta |
| Arrumar os crashes em GPUs Mali | Alta |
| Suporte para Nintendo Switch  | Média |
| Atualizar para o núcleo mais recente | Alta |
| Atualizar o design para Material expressive | Baixa |
| Migrar para Kotlin | Média | 


## Por que existem arquivos .js e .jsx?

Originalmente como uma ideia curiosa, na verdade, as telas do React Native eram apenas uma experimento, eu decidi manter  extremamente básicos, serão finalizados em uma branch separada (armsx2-rn) ou removidos completamente, Não afetam performance já que são escondidos por padrão e não executados. Qualquer PR para os mesmo são bem vindas!

### Para começar a desenvolver com ARMSX2 agora faça o seguinte:

1. Primeiro instale as deps:
```sh
(npm/pnpm/bun) install
```


2. Compilar ARMSX2 com o react native core:
```sh
./gradlew assembleDebug -PenableRN=true
```

E agora você terá um novo botão aparecendo no canto superior direito da tela de carregamento de jogos, clique e comece a desenvolver com hot reload e veja suas mudanças sem recompilamento(nota: compilar agora muda o emucore de static para shared).

