/*
 * Copyright  2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mobileer.androidfxlab

import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.switchmaterial.SwitchMaterial
import com.mobileer.androidfxlab.datatype.Effect
import java.util.*
import kotlin.concurrent.timerTask
import kotlin.math.max
import kotlin.math.min

object EffectsAdapter :
    RecyclerView.Adapter<EffectsAdapter.EffectsHolder>() {
    val effectList = arrayListOf<Effect>()
    lateinit var mRecyclerView: RecyclerView

    // This class adapts view in effect_view.xml for Effect class
    class EffectsHolder(val parentView: ViewGroup) : RecyclerView.ViewHolder(parentView) {
        private val layoutContainer: LinearLayout = parentView.findViewById(R.id.effectContainer)
        lateinit var effect: Effect
        private val floatFormat = "%4.2f"
        private var index: Int = -1
        fun bindEffect(bindedEffect: Effect, position: Int) {
            effect = bindedEffect
            index = position
            // Clear all views
            layoutContainer.removeAllViews()
            View.inflate(layoutContainer.context,
                R.layout.effect_header, layoutContainer)
            val header: LinearLayout = layoutContainer.findViewById(R.id.effectHeader)
            val dragHandleView: View = header.findViewById(R.id.cat_card_list_item_drag_handle)
            val checkBoxView: SwitchMaterial = header.findViewById(R.id.effectEnabled)
            val label: TextView = header.findViewById(R.id.effectLabel)
            // Bind header views
            label.text = effect.name
            checkBoxView.isChecked = effectList[index].enable
            checkBoxView.setOnCheckedChangeListener { _, checked ->
                effectList[index].enable = checked
                NativeInterface.enableEffectAt(checked, index)
            }
            dragHandleView.setOnTouchListener { _, event ->
                if (event.action == MotionEvent.ACTION_DOWN) {
                    itemTouchHelper.startDrag(this@EffectsHolder)
                    return@setOnTouchListener true
                }
                false
            }
            header.setOnTouchListener { _, event ->
                if (event.action == MotionEvent.ACTION_DOWN) {
                    itemTouchHelper.startSwipe(this@EffectsHolder)
                    return@setOnTouchListener true
                }
                false
            }
            // Add correct number of SeekBars based on effect
            for (ind in effect.effectDescription.paramValues.withIndex()) {
                val param = ind.value
                val counter = ind.index
                val view = View.inflate(layoutContainer.context,
                    R.layout.param_seek, null)
                layoutContainer.addView(view)
                val paramWrapper: LinearLayout = view.findViewById(R.id.paramWrapper)
                val paramLabelView: TextView = paramWrapper.findViewById(R.id.paramLabel)
                val minLabelView: TextView = paramWrapper.findViewById(R.id.minLabel)
                val maxLabelView: TextView = paramWrapper.findViewById(R.id.maxLabel)
                val curLabelView: TextView = paramWrapper.findViewById(R.id.curLabel)
                val seekBar: SeekBar = paramWrapper.findViewById(R.id.seekBar)
                paramLabelView.text = param.paramName
                minLabelView.text = floatFormat.format(param.minValue)
                maxLabelView.text = floatFormat.format(param.maxValue)
                seekBar.progress =
                    ((effectList[index].paramValues[counter] - param.minValue) * 100 / (param.maxValue
                            - param.minValue)).toInt()
                curLabelView.text = floatFormat.format(effectList[index].paramValues[counter])
                // Bind param listeners to effects
                seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                    val paramInd = counter
                    var timer : Timer? = null

                    override fun onStartTrackingTouch(p0: SeekBar?) {}

                    override fun onStopTrackingTouch(seekbar: SeekBar?) {}

                    override fun onProgressChanged(
                        seekBar: SeekBar?, progress: Int, fromUser: Boolean
                    ) {
                        val fracprogress =
                            ((seekBar!!.progress / 100f) * (param.maxValue - param.minValue) + param.minValue)
                        curLabelView.text = floatFormat.format(fracprogress)

                        timer?.cancel()
                        timer = Timer()
                        timer?.schedule(timerTask { updateEffectParam(fracprogress) }, 100)
                    }

                    fun updateEffectParam(fracprogress : Float){
                        effectList[index].paramValues[paramInd] = fracprogress
                        NativeInterface.updateParamsAt(effect, index)
                    }
                })
            }
        }

    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): EffectsHolder {
        val myView = LayoutInflater.from(parent.context)
            .inflate(R.layout.effect_view, parent, false)
        return EffectsHolder(myView as ViewGroup)
    }

    override fun onBindViewHolder(holder: EffectsHolder, position: Int) {
        holder.bindEffect(effectList[position], position)
    }

    override fun getItemCount() = effectList.size

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        mRecyclerView = recyclerView
        itemTouchHelper.attachToRecyclerView(mRecyclerView)
    }

    private val itemTouchHelper = ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(
        ItemTouchHelper.UP or ItemTouchHelper.DOWN, ItemTouchHelper.RIGHT or ItemTouchHelper.LEFT
    ) {

        var dragFrom = -1
        var dragTo = -1
        override fun onMove(
            recyclerView: RecyclerView,
            viewHolder: RecyclerView.ViewHolder,
            target: RecyclerView.ViewHolder
        ): Boolean {

            val fromPos = viewHolder.adapterPosition
            val toPos = target.adapterPosition
            if (dragFrom == -1) {
                dragFrom = fromPos
            }
            dragTo = toPos
            effectList.add(toPos, effectList.removeAt(fromPos))
            recyclerView.adapter?.notifyItemMoved(fromPos, toPos)
            return true
        }

        override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
            val position = viewHolder.adapterPosition
            effectList.removeAt(position)
            mRecyclerView.adapter?.notifyItemRemoved(position)
            NativeInterface.removeEffectAt(position)
            for (i in position until effectList.size) {
                var holder = mRecyclerView.findViewHolderForAdapterPosition(i) as EffectsHolder
                holder.bindEffect(holder.effect, i)
            }
        }

        override fun clearView(
            recyclerView: RecyclerView,
            viewHolder: RecyclerView.ViewHolder
        ) {
            super.clearView(recyclerView, viewHolder)
            if (dragFrom != -1 && dragTo != -1 && dragFrom != dragTo) {
                rotateItems(dragFrom, dragTo)
            }
            dragFrom = -1
            dragTo = -1
        }

        override fun isLongPressDragEnabled(): Boolean = false
        override fun isItemViewSwipeEnabled(): Boolean = false

        fun rotateItems(fromPos: Int, toPos: Int) {
            val a = min(fromPos, toPos)
            val b = max(fromPos, toPos)
            for (i in a..b) {
                var holder = mRecyclerView.findViewHolderForAdapterPosition(i) as EffectsHolder
                holder.bindEffect(holder.effect, i)
            }
            NativeInterface.rotateEffectAt(fromPos, toPos)
        }
    })

}
