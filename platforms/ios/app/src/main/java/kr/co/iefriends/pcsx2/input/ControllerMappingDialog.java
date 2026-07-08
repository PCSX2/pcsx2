
/*

By MoonPower (Momo-AUX1) GPLv3 License
   This file is part of ARMSX2.

   ARMSX2 is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ARMSX2 is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ARMSX2.  If not, see <http://www.gnu.org/licenses/>.

*/

package kr.co.iefriends.pcsx2.input;

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.view.InputDevice;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.util.EnumMap;
import java.util.Map;

import kr.co.iefriends.pcsx2.R;

public class ControllerMappingDialog extends DialogFragment {

    private MappingAdapter adapter;
    private ControllerMappingManager.Action waitingForAction;
    private TextView waitingView;

    @NonNull
    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        Context context = requireContext();
        ControllerMappingManager.init(context);

        View view = LayoutInflater.from(context).inflate(R.layout.dialog_controller_mapping, null, false);
        waitingView = view.findViewById(R.id.tv_waiting_for_input);

        RecyclerView recyclerView = view.findViewById(R.id.rv_controller_mapping);
        recyclerView.setLayoutManager(new LinearLayoutManager(context));
        adapter = new MappingAdapter(context, this::beginRebind, this::clearAction);
        recyclerView.setAdapter(adapter);

        AlertDialog dialog = new MaterialAlertDialogBuilder(context)
                .setTitle(R.string.controller_mapping_title)
                .setView(view)
                .setPositiveButton(android.R.string.ok, null)
                .setNeutralButton(R.string.controller_mapping_reset, (d, which) -> {
                    ControllerMappingManager.resetToDefaults();
                    if (adapter != null) adapter.updateMappings();
                    cancelWaiting();
                })
                .create();

        dialog.setOnKeyListener((d, keyCode, event) -> handleKeyEvent(keyCode, event));
        View decor = dialog.getWindow() != null ? dialog.getWindow().getDecorView() : null;
        if (decor != null) {
            decor.setOnGenericMotionListener((v, event) -> handleMotionEvent(event));
        }
        return dialog;
    }

    private void beginRebind(@NonNull ControllerMappingManager.Action action) {
        waitingForAction = action;
        if (waitingView != null) {
            String label = requireContext().getString(action.getLabelRes());
            waitingView.setText(getString(R.string.controller_mapping_waiting) + " (" + label + ")");
            waitingView.setVisibility(View.VISIBLE);
        }
    }

    private void clearAction(@NonNull ControllerMappingManager.Action action) {
        ControllerMappingManager.clear(action);
        if (adapter != null) {
            adapter.updateMappings();
        }
    }

    private boolean handleKeyEvent(int keyCode, @NonNull KeyEvent event) {
        if (waitingForAction == null) {
            return false;
        }
        if (event.getAction() != KeyEvent.ACTION_DOWN || event.getRepeatCount() != 0) {
            return true;
        }
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            cancelWaiting();
            return true;
        }
        ControllerMappingManager.assign(waitingForAction, keyCode);
        if (adapter != null) {
            adapter.updateMappings();
        }
        cancelWaiting();
        return true;
    }

    private boolean handleMotionEvent(@NonNull MotionEvent event) {
        if (waitingForAction == null) return false;
        if (event.getAction() != MotionEvent.ACTION_MOVE) return false;
        if (!event.isFromSource(InputDevice.SOURCE_JOYSTICK) && !event.isFromSource(InputDevice.SOURCE_GAMEPAD)) {
            return false;
        }

        float l2 = event.getAxisValue(MotionEvent.AXIS_LTRIGGER);
        if (l2 == 0f) l2 = event.getAxisValue(MotionEvent.AXIS_BRAKE);
        float r2 = event.getAxisValue(MotionEvent.AXIS_RTRIGGER);
        if (r2 == 0f) r2 = event.getAxisValue(MotionEvent.AXIS_GAS);

        final float threshold = 0.5f;
        int keyCode = ControllerMappingManager.NO_MAPPING;
        if (l2 > threshold) {
            keyCode = KeyEvent.KEYCODE_BUTTON_L2;
        } else if (r2 > threshold) {
            keyCode = KeyEvent.KEYCODE_BUTTON_R2;
        }

        if (keyCode == ControllerMappingManager.NO_MAPPING) {
            return false;
        }

        ControllerMappingManager.assign(waitingForAction, keyCode);
        if (adapter != null) {
            adapter.updateMappings();
        }
        cancelWaiting();
        return true;
    }

    private void cancelWaiting() {
        waitingForAction = null;
        if (waitingView != null) {
            waitingView.setVisibility(View.GONE);
        }
    }

    private static class MappingAdapter extends RecyclerView.Adapter<MappingAdapter.MappingViewHolder> {
        private final Context context;
        private final Map<ControllerMappingManager.Action, Integer> currentMappings = new EnumMap<>(ControllerMappingManager.Action.class);
        private final ActionListener listener;
        private final ClearListener clearListener;

        MappingAdapter(Context context, ActionListener listener, ClearListener clearListener) {
            this.context = context;
            this.listener = listener;
            this.clearListener = clearListener;
            updateMappings();
        }

        void updateMappings() {
            currentMappings.clear();
            currentMappings.putAll(ControllerMappingManager.getCurrentMappings());
            notifyDataSetChanged();
        }

        @NonNull
        @Override
        public MappingViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View itemView = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_controller_mapping, parent, false);
            return new MappingViewHolder(itemView);
        }

        @Override
        public void onBindViewHolder(@NonNull MappingViewHolder holder, int position) {
            ControllerMappingManager.Action action = ControllerMappingManager.Action.values()[position];
            holder.bind(action, getBindingDisplay(action));
        }

        private String getBindingDisplay(ControllerMappingManager.Action action) {
            Integer keyCode = currentMappings.get(action);
            if (keyCode == null) {
                keyCode = action.getDefaultKeyCode();
            }
            if (keyCode == ControllerMappingManager.NO_MAPPING) {
                return context.getString(R.string.controller_mapping_unbound);
            }
            String display = ControllerMappingManager.keyCodeToDisplay(keyCode);
            if (display == null || display.isEmpty()) {
                display = context.getString(R.string.controller_mapping_unbound);
            }
            return display;
        }

        @Override
        public int getItemCount() {
            return ControllerMappingManager.Action.values().length;
        }

        class MappingViewHolder extends RecyclerView.ViewHolder {
            private final TextView actionName;
            private final TextView actionBinding;
            private final View rebindButton;
            private final View clearButton;

            MappingViewHolder(@NonNull View itemView) {
                super(itemView);
                actionName = itemView.findViewById(R.id.tv_action_name);
                actionBinding = itemView.findViewById(R.id.tv_action_binding);
                rebindButton = itemView.findViewById(R.id.btn_rebind);
                clearButton = itemView.findViewById(R.id.btn_clear);
            }

            void bind(ControllerMappingManager.Action action, String bindingDisplay) {
                actionName.setText(context.getString(action.getLabelRes()));
                actionBinding.setText(bindingDisplay);
                rebindButton.setOnClickListener(v -> listener.onSelect(action));
                clearButton.setOnClickListener(v -> clearListener.onClear(action));
            }
        }

        interface ActionListener {
            void onSelect(ControllerMappingManager.Action action);
        }

        interface ClearListener {
            void onClear(ControllerMappingManager.Action action);
        }
    }
}
