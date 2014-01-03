#!/usr/bin/env pypy
# -*- coding: utf-8 -*-
# vim: set et sw=4 ts=4 sts=4 ff=unix fenc=utf8:

import re
import wave
import numpy
import redis

SP_COUNT = 5
BINARY_COUNT = 10
REDIS = {
    'host': '127.0.0.1',
    'port': 6300,
    'db': 1
}

def read_lrc(file):
    f = open(file, 'rb')
    lines = f.readlines()
    f.close()

    times = []

    for line in lines:
        line = line.strip()
        if not line:
            continue
        match = re.match(r'\[(\d*):([\d\.]*)\](.*)', line)
        if not match:
            continue
        groups = match.groups()
        time = int(groups[0]) * 60 + float(groups[1])
        times.append(time)

    length = len(times)
    nd_times = numpy.zeros((length - 1) * SP_COUNT + 1)
    index = 0
    for idx in xrange(0, length - 1):
        time = times[idx]
        nextime = times[idx + 1]
        gap_time = (nextime - time) / SP_COUNT
        for t in xrange(0, SP_COUNT):
            g_time = time + t * gap_time
            nd_times[index] = g_time
            index = index + 1
    nd_times[index] = times[length - 1]

    return nd_times


def wave_form(wav_file, nd_times):
    length = len(nd_times)
    waves = numpy.zeros(length)

    f = wave.open(wav_file, 'rb')
    params = f.getparams()
    nchannels, sampwidth, framerate, nframes = params[:4]
    frames = f.readframes(nframes)
    f.close()

    data = numpy.fromstring(frames, numpy.int16)
    maxframe = max(abs(data))
    data = data * 1.0 / maxframe

    index = 0
    for idx in xrange(0, length - 1):
        time = int(nd_times[idx] * framerate)
        nextime = int(nd_times[idx + 1] * framerate)
        cur_frame = data[time:nextime]
        waves[index] = numpy.sum(numpy.abs(cur_frame)) / (nextime - time)
        index = index + 1
    time = int(nd_times[length - 2] * framerate)
    nextime = int(nd_times[length - 1] * framerate)
    cur_frame = data[time:nextime]
    waves[index] = numpy.sum(numpy.abs(cur_frame)) / (nextime - time)

    mean = numpy.mean(waves)
    minimal = min(waves)
    maximal = max(waves)

    return waves, mean, minimal, maximal, maxframe


def binary_zation(waves, mean, minimal, maximal):
    top = maximal - mean
    bottom = mean - minimal

    top_gap = top * 2 / BINARY_COUNT
    bottom_gap = bottom * 2 / BINARY_COUNT
    length = len(waves)
    binarys = numpy.zeros(length)

    for idx in xrange(0, length):
        if waves[idx] > mean:
            num_gap = int((waves[idx] - mean) / top_gap)
            binarys[idx] = BINARY_COUNT / 2 + num_gap + 1
        else:
            num_gap = int((mean - waves[idx]) / bottom_gap)
            binarys[idx] = BINARY_COUNT / 2 - num_gap + 1

    return binarys


def binary_to_redis(musicno, nd_times, waves, mean, minimal, maximal, maxframe):
    r = redis.Redis(host=REDIS['host'], port=REDIS['port'], db=REDIS['db'])
    key = 'music_binary:%s' % musicno
    r.hmset(key, {
        'x_axis': nd_times.tolist(),
        'y_axis': waves.tolist(),
        'mean': mean,
        'min': minimal,
        'max': maximal,
        'maxframe': maxframe
    })


def draw_plot(waves, nd_times):
    import matplotlib
    matplotlib.use('agg')
    import pylab

    pylab.plot(nd_times, waves)
    pylab.xlabel('time (second)')
    pylab.ylabel('binarization')
    pylab.savefig('test.png', dpi=1200)


if __name__ == '__main__':
    nd_times = read_lrc('7013785.lrc')
    data = wave_form('7013785.wav', nd_times)
    binarys = binary_zation(*data[0:-1])
    binary_to_redis(7013785, nd_times, binarys, data[1], data[2], data[3], data[4])

