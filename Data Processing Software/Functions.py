# This script serves as an accummilation of the functions developed within the project:
# Investigating the Design of a  Photoplethysmography Device for Clinical Applications
# By A. Aspeling, 2024

#========== LIBRARIES ==========#
import numpy as np
from scipy.signal import find_peaks
from scipy.interpolate import interp1d
from scipy.signal import resample

#========== FUNCTIONS ==========#
# Filter Peaks
def filter_peaks_and_troughs(chunk):
    filtered_peaks_ind = []
    filtered_peaks_val = []
    filtered_troughs_ind = []
    filtered_troughs_val = []

    # Get the peaks and troughs
    chunk_neg = -chunk
    
    peaks_ind = find_peaks(chunk, height=[np.quantile(chunk, 0.75), max(chunk)])[0]
    troughs_ind = find_peaks(chunk_neg, height=[np.quantile(chunk_neg, 0.75), max(chunk_neg)])[0]

    peaks_val = chunk[peaks_ind]
    troughs_val = chunk[troughs_ind]

    peak_i = 0
    trough_i = 0
    last_was_peak = False

    while peak_i < len(peaks_ind) and trough_i < len(troughs_ind):
        peak1_ind = peaks_ind[peak_i]
        peak1_val = peaks_val[peak_i]
        trough1_ind = troughs_ind[trough_i]
        trough1_val = troughs_val[trough_i]

        if peak1_ind < trough1_ind:
            if not last_was_peak:
                filtered_peaks_ind.append(peak1_ind)
                filtered_peaks_val.append(peak1_val)
                last_was_peak = True
            peak_i += 1
        else:
            if last_was_peak:
                filtered_troughs_ind.append(trough1_ind)
                filtered_troughs_val.append(trough1_val)
                last_was_peak = False
            trough_i += 1

    # Handle remaining peaks and troughs
    while peak_i < len(peaks_ind):
        if not last_was_peak:
            filtered_peaks_ind.append(peaks_ind[peak_i])
            filtered_peaks_val.append(peaks_val[peak_i])
            last_was_peak = True
        peak_i += 1

    while trough_i < len(troughs_ind):
        if last_was_peak:
            filtered_troughs_ind.append(troughs_ind[trough_i])
            filtered_troughs_val.append(troughs_val[trough_i])
            last_was_peak = False
        trough_i += 1

    return filtered_peaks_ind

# Align Signals
def align_signals(sig1, sig2, peaks1, peaks2, tim1, tim2):
    shift = peaks1[0] - peaks2[0]

    if shift < 0:
        # This means sig1 is in front
        pos_shift = (-1)*(shift)

        aligned_sig1 = sig1[:(-pos_shift)]
        aligned_tim1 = tim1[:(-pos_shift)]

        aligned_sig2 = sig2[pos_shift:]
        aligned_tim2 = tim2[pos_shift:]

        # Remove all sig1 values that are still before sig2
        start_tim2 = aligned_tim2[0]
        ind_remove1 = 0

        for i in range(len(aligned_tim1)):
            if aligned_tim1[i] >= start_tim2:
                ind_remove1 = i
                break

        aligned_sig1 = aligned_sig1[ind_remove1:]
        aligned_tim1 = aligned_tim1[ind_remove1:]

        aligned_sig2 = aligned_sig2[:(-ind_remove1)]
        aligned_tim2 = aligned_tim2[:(-ind_remove1)]

        return aligned_sig1, aligned_sig2, aligned_tim1, aligned_tim2
    
    elif shift > 0:
        # This means sig2 is in front
        aligned_sig1 = sig1[shift:]
        aligned_tim1 = tim1[shift:]

        aligned_sig2 = sig2[:(-shift)]
        aligned_tim2 = tim2[:(-shift)]

        return aligned_sig1, aligned_sig2, aligned_tim1, aligned_tim2

    else:
        return sig1, sig2, tim1, tim2

# Split Datapoint Intervals
def split_intervals(data, time):
    start_i = 0
    end_i = 0

    interval_data = []
    interval_time = []

    # Set selected intervals
    interval_sections_time = [70000.00, 90000.00] 

    interval_num = 1

    while interval_num <= 2:
        for i in range(0,len(time)):
            if time[i] >= interval_sections_time[interval_num - 1]:
                start_i = i
                break

        end_i = start_i + 800

        interval_data.append(data[start_i:end_i])
        interval_time.append(time[start_i:end_i])

        interval_num += 1

    return interval_data, interval_time

# Segment Data Using Interpolation
def interpolate_data(data, seg_size, seg_peaks_num):
    peak_ind_selected = 0
    segmented_data = []

    peaks_ind = filter_peaks_and_troughs(data)

    if len(peaks_ind) > 0:
        peaks_on_side = round(seg_peaks_num/2)

        valid_start = peaks_on_side + 1
        valid_end = len(peaks_ind) - peaks_on_side - 2
        valid_indices = range(valid_start, valid_end + 1)

        # Ensure there is a valid range to choose from
        if len(valid_indices) == 0:
            raise ValueError("No valid peak can be chosen with the given constraints")

        # Select random peak
        peak_ind_selected = np.random.choice(valid_indices)

        # Get full segment by making sure only x-amount of peaks are collected
        lower_ind = peaks_ind[peak_ind_selected - peaks_on_side - 1]
        upper_ind = peaks_ind[peak_ind_selected + peaks_on_side + 1]

        lower_diff = peaks_ind[peak_ind_selected] - lower_ind
        upper_diff = upper_ind - peaks_ind[peak_ind_selected]

        # Check if code missed peaks in between (10 ind == 0.18s/5Hz)
        if lower_diff >= (upper_diff + 10) or upper_diff >= (lower_diff + 10):
            min_diff = min(lower_diff, upper_diff)
            lower_ind = peaks_ind[peak_ind_selected] - min_diff
            upper_ind = peaks_ind[peak_ind_selected] + min_diff

        segment = data[lower_ind:upper_ind]

        # Reshape data
        original_length = len(segment)

        if original_length != 1:
            if original_length == seg_size:
                reshaped_segment = segment

            elif original_length < seg_size:
                # Interpolate to get the desired length
                new_indices = np.linspace(0, original_length - 1, seg_size)
                interpolator = interp1d(np.arange(original_length), segment, kind='linear')
                interpolated_data = interpolator(new_indices)
                reshaped_segment = interpolated_data

            else:
                # Resample to get the desired length
                reshaped_segment = resample(segment, seg_size)

            segmented_data.append(reshaped_segment)

        else:
            segmented_data.append([])

    return segmented_data

# Segment Data Using Padding
def pad_data(data, seg_size, seg_peaks_num):
    peak_ind_selected = 0
    segmented_data = []

    peaks_ind = filter_peaks_and_troughs(data)

    if len(peaks_ind) > 0:
        peaks_on_side = round(seg_peaks_num/2)

        valid_start = peaks_on_side + 1
        valid_end = len(peaks_ind) - peaks_on_side - 2
        valid_indices = range(valid_start, valid_end + 1)

        # Ensure there is a valid range to choose from
        if len(valid_indices) == 0:
            raise ValueError("No valid peak can be chosen with the given constraints")

        # Select random peak
        peak_ind_selected = np.random.choice(valid_indices)

        # Get full segment by making sure only x-amount of peaks are collected
        lower_ind = peaks_ind[peak_ind_selected - peaks_on_side - 1]
        upper_ind = peaks_ind[peak_ind_selected + peaks_on_side + 1]

        lower_diff = peaks_ind[peak_ind_selected] - lower_ind
        upper_diff = upper_ind - peaks_ind[peak_ind_selected]

        ## Check if code missed peaks in between (10 ind == 0.18s/5Hz)
        if lower_diff >= (upper_diff + 10) or upper_diff >= (lower_diff + 10):
            min_diff = min(lower_diff, upper_diff)
            lower_ind = peaks_ind[peak_ind_selected] - min_diff
            upper_ind = peaks_ind[peak_ind_selected] + min_diff

        segment = data[lower_ind:upper_ind]

        # Reshape data
        original_length = len(segment)

        if original_length != 1:
            if original_length == seg_size:
                reshaped_segment = segment

            elif original_length < seg_size:
                # Pad to get the desired length
                reshaped_segment = [0]*seg_size
                for k in range(original_length):
                    reshaped_segment[k] = segment[k]

            else:
                # Cut off data to the desired length
                reshaped_segment = segment[:seg_size]

            segmented_data.append(reshaped_segment)

        else:
            segmented_data.append([])

    return segmented_data

# Clip Data
def clip_data(data, seg_size):
    peak_ind_selected = 0
    segmented_data = []

    peaks_ind = filter_peaks_and_troughs(data)

    if len(peaks_ind) > 0:
        sizes_on_side = round(seg_size/2)

        for j in range(len(peaks_ind)):
            if peaks_ind[j] >=  sizes_on_side:
                valid_start = j
                break
        
        for j in range(len(peaks_ind)):
            if peaks_ind[j] <= (len(data) - sizes_on_side):
                valid_end = j
            else:
                break

        valid_indices = range(valid_start, valid_end)

        if valid_start == valid_end:
            peak_ind_selected = valid_end
        
        else:
            # Ensure there is a valid range to choose from
            if len(valid_indices) == 0:
                raise ValueError("No valid peak can be chosen with the given constraints")

            # Select random peak
            peak_ind_selected = np.random.choice(valid_indices)

        # Get full segment by making sure only x-amount of peaks are collected
        lower_ind = peaks_ind[peak_ind_selected] - sizes_on_side
        upper_ind = peaks_ind[peak_ind_selected] + sizes_on_side

        segmented_data = data[lower_ind:upper_ind]

    return segmented_data