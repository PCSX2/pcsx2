/* SPU2-X, A plugin for Emulating the Sound Processing Unit of the Playstation 2
 * Developed and maintained by the Pcsx2 Development Team.
 *
 * SPU2-X is free software: you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * SPU2-X is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with SPU2-X.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Global.h"
#include "Dialogs.h"

bool AdvancedVolumeControl = false;

float VolumeAdjustFLdb; // Decibels settings, cos audiophiles love that.
float VolumeAdjustCdb;
float VolumeAdjustFRdb;
float VolumeAdjustBLdb;
float VolumeAdjustBRdb;
float VolumeAdjustSLdb;
float VolumeAdjustSRdb;
float VolumeAdjustLFEdb;
float VolumeAdjustFL; // Linear coefs calculated from decibels.
float VolumeAdjustC;
float VolumeAdjustFR;
float VolumeAdjustBL;
float VolumeAdjustBR;
float VolumeAdjustSL;
float VolumeAdjustSR;
float VolumeAdjustLFE;

void AdvancedVolumeConfig::ReadSettings()
{
    AdvancedVolumeControl = CfgReadBool(L"MIXING", L"AdvancedVolumeControl", false);

    VolumeAdjustCdb = CfgReadFloat(L"MIXING", L"VolumeAdjustC(dB)", 0);
    VolumeAdjustFLdb = CfgReadFloat(L"MIXING", L"VolumeAdjustFL(dB)", 0);
    VolumeAdjustFRdb = CfgReadFloat(L"MIXING", L"VolumeAdjustFR(dB)", 0);
    VolumeAdjustBLdb = CfgReadFloat(L"MIXING", L"VolumeAdjustBL(dB)", 0);
    VolumeAdjustBRdb = CfgReadFloat(L"MIXING", L"VolumeAdjustBR(dB)", 0);
    VolumeAdjustSLdb = CfgReadFloat(L"MIXING", L"VolumeAdjustSL(dB)", 0);
    VolumeAdjustSRdb = CfgReadFloat(L"MIXING", L"VolumeAdjustSR(dB)", 0);
    VolumeAdjustLFEdb = CfgReadFloat(L"MIXING", L"VolumeAdjustLFE(dB)", 0);

    VolumeAdjustC = powf(10, VolumeAdjustCdb / 10);
    VolumeAdjustFL = powf(10, VolumeAdjustFLdb / 10);
    VolumeAdjustFR = powf(10, VolumeAdjustFRdb / 10);
    VolumeAdjustBL = powf(10, VolumeAdjustBLdb / 10);
    VolumeAdjustBR = powf(10, VolumeAdjustBRdb / 10);
    VolumeAdjustSL = powf(10, VolumeAdjustSLdb / 10);
    VolumeAdjustSR = powf(10, VolumeAdjustSRdb / 10);
    VolumeAdjustLFE = powf(10, VolumeAdjustLFEdb / 10);
}

void AdvancedVolumeConfig::WriteSettings()
{
    CfgWriteBool(L"MIXING", L"AdvancedVolumeControl", AdvancedVolumeControl);

    CfgWriteFloat(L"MIXING", L"VolumeAdjustC(dB)", VolumeAdjustCdb);
    CfgWriteFloat(L"MIXING", L"VolumeAdjustFL(dB)", VolumeAdjustFLdb);
    CfgWriteFloat(L"MIXING", L"VolumeAdjustFR(dB)", VolumeAdjustFRdb);
    CfgWriteFloat(L"MIXING", L"VolumeAdjustBL(dB)", VolumeAdjustBLdb);
    CfgWriteFloat(L"MIXING", L"VolumeAdjustBR(dB)", VolumeAdjustBRdb);
    CfgWriteFloat(L"MIXING", L"VolumeAdjustSL(dB)", VolumeAdjustSLdb);
    CfgWriteFloat(L"MIXING", L"VolumeAdjustSR(dB)", VolumeAdjustSRdb);
    CfgWriteFloat(L"MIXING", L"VolumeAdjustLFE(dB)", VolumeAdjustLFEdb);
}

static BOOL CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int wmId;
    wchar_t temp[384] = {0};

    switch (uMsg) {
        case WM_PAINT:
            return FALSE;

        case WM_INITDIALOG: {
            INIT_SLIDER(IDC_VOLADJ_CDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_CDB, TBM_SETPOS, TRUE, VolumeAdjustCdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustCdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_C_LABEL), temp);

            INIT_SLIDER(IDC_VOLADJ_FLDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_FLDB, TBM_SETPOS, TRUE, VolumeAdjustFLdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustFLdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_FL_LABEL), temp);

            INIT_SLIDER(IDC_VOLADJ_FRDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_FRDB, TBM_SETPOS, TRUE, VolumeAdjustFRdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustFRdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_FR_LABEL), temp);

            INIT_SLIDER(IDC_VOLADJ_BLDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_BLDB, TBM_SETPOS, TRUE, VolumeAdjustBLdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustBLdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_BL_LABEL), temp);

            INIT_SLIDER(IDC_VOLADJ_BRDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_BRDB, TBM_SETPOS, TRUE, VolumeAdjustBRdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustBRdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_BR_LABEL), temp);

            INIT_SLIDER(IDC_VOLADJ_SLDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_SLDB, TBM_SETPOS, TRUE, VolumeAdjustSLdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustSLdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_SL_LABEL), temp);

            INIT_SLIDER(IDC_VOLADJ_SRDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_SRDB, TBM_SETPOS, TRUE, VolumeAdjustSRdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustSRdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_SR_LABEL), temp);

            INIT_SLIDER(IDC_VOLADJ_LFEDB, 0, 50, 5, 5, 1);
            SendDialogMsg(hWnd, IDC_VOLADJ_LFEDB, TBM_SETPOS, TRUE, VolumeAdjustLFEdb + 25.0f);
            swprintf_s(temp, L"%d dB", (int)VolumeAdjustLFEdb);
            SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_LFE_LABEL), temp);
        }

        case WM_COMMAND:
            wmId = LOWORD(wParam);
            if (wmId == IDOK) {
                VolumeAdjustCdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_CDB, TBM_GETPOS, 0, 0) - 25.0f);
                VolumeAdjustFLdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_FLDB, TBM_GETPOS, 0, 0) - 25.0f);
                VolumeAdjustFRdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_FRDB, TBM_GETPOS, 0, 0) - 25.0f);
                VolumeAdjustBLdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_BLDB, TBM_GETPOS, 0, 0) - 25.0f);
                VolumeAdjustBRdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_BRDB, TBM_GETPOS, 0, 0) - 25.0f);
                VolumeAdjustSLdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_SLDB, TBM_GETPOS, 0, 0) - 25.0f);
                VolumeAdjustSRdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_SRDB, TBM_GETPOS, 0, 0) - 25.0f);
                VolumeAdjustLFEdb = ((float)SendDialogMsg(hWnd, IDC_VOLADJ_LFEDB, TBM_GETPOS, 0, 0) - 25.0f);

                WriteSettings();
                EndDialog(hWnd, 0);
            } else if (wmId == IDC_RESET_DEFAULTS) {
                SendDialogMsg(hWnd, IDC_VOLADJ_CDB, TBM_SETPOS, TRUE, 25.0f);
                swprintf_s(temp, L"%d dB", 0);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_C_LABEL), temp);
                SendDialogMsg(hWnd, IDC_VOLADJ_FLDB, TBM_SETPOS, TRUE, 25.0f);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_FL_LABEL), temp);
                SendDialogMsg(hWnd, IDC_VOLADJ_FRDB, TBM_SETPOS, TRUE, 25.0f);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_FR_LABEL), temp);
                SendDialogMsg(hWnd, IDC_VOLADJ_BLDB, TBM_SETPOS, TRUE, 25.0f);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_BL_LABEL), temp);
                SendDialogMsg(hWnd, IDC_VOLADJ_BRDB, TBM_SETPOS, TRUE, 25.0f);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_BR_LABEL), temp);
                SendDialogMsg(hWnd, IDC_VOLADJ_SLDB, TBM_SETPOS, TRUE, 25.0f);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_SL_LABEL), temp);
                SendDialogMsg(hWnd, IDC_VOLADJ_SRDB, TBM_SETPOS, TRUE, 25.0f);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_SR_LABEL), temp);
                SendDialogMsg(hWnd, IDC_VOLADJ_LFEDB, TBM_SETPOS, TRUE, 25.0f);
                SetWindowText(GetDlgItem(hWnd, IDC_ADV_VOL_LFE_LABEL), temp);
            } else if (wmId == IDCANCEL) {
                EndDialog(hWnd, 0);
            }
            break;

        case WM_HSCROLL: {
            wmId = LOWORD(wParam);
            HWND hwndDlg = (HWND)lParam;

            int curpos = HIWORD(wParam);

            switch (wmId) {
                case TB_LINEUP:
                case TB_LINEDOWN:
                case TB_PAGEUP:
                case TB_PAGEDOWN:
                case TB_TOP:
                case TB_BOTTOM:
                    curpos = (int)SendMessage(hwndDlg, TBM_GETPOS, 0, 0);

                case TB_THUMBPOSITION:
                case TB_THUMBTRACK:
                    Clampify(curpos,
                             (int)SendMessage(hwndDlg, TBM_GETRANGEMIN, 0, 0),
                             (int)SendMessage(hwndDlg, TBM_GETRANGEMAX, 0, 0));

                    SendMessage((HWND)lParam, TBM_SETPOS, TRUE, curpos);

                    if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_CDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_C_LABEL, temp);
                    } else if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_FLDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_FL_LABEL, temp);
                    } else if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_FRDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_FR_LABEL, temp);
                    } else if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_BLDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_BL_LABEL, temp);
                    } else if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_BRDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_BR_LABEL, temp);
                    } else if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_SLDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_SL_LABEL, temp);
                    } else if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_SRDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_SR_LABEL, temp);
                    } else if (hwndDlg == GetDlgItem(hWnd, IDC_VOLADJ_LFEDB)) {
                        swprintf_s(temp, L"%d dB", curpos - 25);
                        SetDlgItemText(hWnd, IDC_ADV_VOL_LFE_LABEL, temp);
                    }
                    break;

                default:
                    return FALSE;
            }
        } break;

        default:
            return FALSE;
    }
    return TRUE;
}

void AdvancedVolumeConfig::OpenDialog()
{
    INT_PTR ret = DialogBox(hInstance, MAKEINTRESOURCE(IDD_CONFIG_ADVANCEDVOLUME), GetActiveWindow(), (DLGPROC)DialogProc);
    if (ret == -1) {
        MessageBox(GetActiveWindow(), L"Error Opening the Advanced Volume Control configuration dialog.", L"OMG ERROR!", MB_OK | MB_SETFOREGROUND);
        return;
    }
    ReadSettings();
}
