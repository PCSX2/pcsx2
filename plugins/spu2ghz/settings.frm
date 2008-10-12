VERSION 5.00
Begin VB.Form Form1 
   BorderStyle     =   3  'Fixed Dialog
   Caption         =   "SPU2ghz Settings"
   ClientHeight    =   5055
   ClientLeft      =   45
   ClientTop       =   435
   ClientWidth     =   5655
   Icon            =   "settings.frx":0000
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   5055
   ScaleWidth      =   5655
   ShowInTaskbar   =   0   'False
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton Command2 
      Caption         =   "OK"
      Default         =   -1  'True
      Height          =   375
      Left            =   3000
      TabIndex        =   24
      Top             =   4560
      Width           =   1215
   End
   Begin VB.CommandButton Command1 
      Cancel          =   -1  'True
      Caption         =   "Cancel"
      Height          =   375
      Left            =   4320
      TabIndex        =   23
      Top             =   4560
      Width           =   1215
   End
   Begin VB.Frame Frame6 
      Caption         =   "Effects (Reverb) Settings"
      Height          =   615
      Left            =   2880
      TabIndex        =   17
      Top             =   1440
      Width           =   2655
      Begin VB.CheckBox Check10 
         Caption         =   "Enable Effect Processing"
         Height          =   255
         Left            =   120
         TabIndex        =   18
         Top             =   240
         Width           =   2415
      End
   End
   Begin VB.Frame Frame5 
      Caption         =   "Output Settings"
      Height          =   1215
      Left            =   2880
      TabIndex        =   16
      Top             =   120
      Width           =   2655
      Begin VB.ComboBox Combo2 
         Height          =   315
         Left            =   1440
         TabIndex        =   20
         Text            =   "48000"
         Top             =   240
         Width           =   1095
      End
      Begin VB.Label Label4 
         Caption         =   "Buffer Size"
         Height          =   255
         Left            =   120
         TabIndex        =   21
         Top             =   600
         Width           =   1095
      End
      Begin VB.Label Label3 
         Caption         =   "Sample Rate"
         Height          =   255
         Left            =   120
         TabIndex        =   19
         Top             =   240
         Width           =   1215
      End
      Begin VB.Label Label5 
         Alignment       =   1  'Right Justify
         Caption         =   "2048"
         Height          =   255
         Left            =   1800
         TabIndex        =   22
         Top             =   840
         Width           =   735
      End
   End
   Begin VB.Frame Frame4 
      Caption         =   "Mixing Settings"
      Height          =   1935
      Left            =   120
      TabIndex        =   11
      Top             =   120
      Width           =   2655
      Begin VB.CheckBox Check9 
         Caption         =   "Use Asynchronous Mixing"
         Height          =   375
         Left            =   120
         TabIndex        =   14
         Top             =   840
         Width           =   2415
      End
      Begin VB.ComboBox Combo1 
         Height          =   315
         Left            =   120
         Style           =   2  'Dropdown List
         TabIndex        =   12
         Top             =   480
         Width           =   2415
      End
      Begin VB.Label Label2 
         Caption         =   "NOTE: The Async. Mixer will use a lot more of cpu power and will have less compatibility."
         Height          =   615
         Left            =   120
         TabIndex        =   15
         Top             =   1200
         Width           =   2415
      End
      Begin VB.Label Label1 
         Caption         =   "Interpolation:"
         Height          =   255
         Left            =   120
         TabIndex        =   13
         Top             =   240
         Width           =   2415
      End
   End
   Begin VB.Frame Frame1 
      Caption         =   "Debug Settings"
      Height          =   2295
      Left            =   120
      TabIndex        =   0
      Top             =   2160
      Width           =   5415
      Begin VB.Frame Frame3 
         Caption         =   "Dumps (on close)"
         Height          =   1215
         Left            =   2760
         TabIndex        =   7
         Top             =   960
         Width           =   2535
         Begin VB.CheckBox Check8 
            Caption         =   "Dump Register Data"
            Height          =   255
            Left            =   120
            TabIndex        =   10
            Top             =   840
            Width           =   2295
         End
         Begin VB.CheckBox Check7 
            Caption         =   "Dump Memory Contents"
            Height          =   375
            Left            =   120
            TabIndex        =   9
            Top             =   480
            Width           =   2295
         End
         Begin VB.CheckBox Check6 
            Caption         =   "Dump Core and Voice State"
            Height          =   255
            Left            =   120
            TabIndex        =   8
            Top             =   240
            Width           =   2295
         End
      End
      Begin VB.Frame Frame2 
         Caption         =   "Logging"
         Height          =   1215
         Left            =   120
         TabIndex        =   3
         Top             =   960
         Width           =   2535
         Begin VB.CheckBox Check5 
            Caption         =   "Log Audio Output"
            Height          =   255
            Left            =   120
            TabIndex        =   6
            Top             =   840
            Width           =   2295
         End
         Begin VB.CheckBox Check4 
            Caption         =   "Log DMA Writes"
            Height          =   375
            Left            =   120
            TabIndex        =   5
            Top             =   480
            Width           =   2295
         End
         Begin VB.CheckBox Check3 
            Caption         =   "Log Register/DMA Actions"
            Height          =   255
            Left            =   120
            TabIndex        =   4
            Top             =   240
            Width           =   2295
         End
      End
      Begin VB.CheckBox Check2 
         Caption         =   "Send Information Messages to the Emulator Console"
         Height          =   255
         Left            =   240
         TabIndex        =   2
         Top             =   600
         Width           =   5055
      End
      Begin VB.CheckBox Check1 
         Caption         =   "Enable Debug Options"
         Height          =   375
         Left            =   240
         TabIndex        =   1
         Top             =   240
         Width           =   3255
      End
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

