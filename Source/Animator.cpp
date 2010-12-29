/*
Copyright (c) 2010 - Wii Banner Player Project

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#include "Animator.h"

namespace WiiBanner
{

void Animator::SetFrame(FrameNumber frame_number)
{
	ForEach(hermite_keys, [=](const std::pair<const KeyType&, const HermiteKeyHandler&> frame_handler)
	{
		const auto& frame_type = frame_handler.first;
		const auto frame_value = frame_handler.second.GetFrame(frame_number);

		ProcessHermiteKey(frame_type, frame_value);
	});

	ForEach(step_keys, [=](const std::pair<const KeyType&, const StepKeyHandler&> frame_handler)
	{
		const auto& frame_type = frame_handler.first;
		auto const frame_data = frame_handler.second.GetFrame(frame_number);

		ProcessStepKey(frame_type, frame_data);
	});
}

void StepKeyHandler::Load(std::istream& file, u16 count)
{
	while (count--)
	{
		FrameNumber frame;
		file >> BE >> frame;

		auto& data = keys[frame];
		file >> BE >> data.data1 >> data.data2;

		file.seekg(2, std::ios::cur);

		//std::cout << "\t\t\t" "frame: " << frame << ' ' << pair.first << " " << pair.second << '\n';
	}
}

void HermiteKeyHandler::Load(std::istream& file, u16 count)
{
	while (count--)
	{
		std::pair<FrameNumber, KeyData> pair;

		// read the frame number
		file >> BE >> pair.first;

		// read the value and slope
		file >> BE >> pair.second.value >> pair.second.slope;

		keys.insert(pair);

		//std::cout << "\t\t\t" "frame: " << frame << ' ' << keys[frame] << '\n';
	}
}

StepKeyHandler::KeyData StepKeyHandler::GetFrame(FrameNumber frame_number) const
{
	// assuming not empty, a safe assumption currently

	// find the current frame, or the one after it
	auto frame_it = keys.lower_bound(frame_number);

	// current frame is higher than any keyframe, use the last keyframe
	if (keys.end() == frame_it)
		--frame_it;

	// if this is after the current frame and not the first keyframe, use the previous one
	if (frame_number < frame_it->first && keys.begin() != frame_it)
		--frame_it;

	return frame_it->second;
}

float HermiteKeyHandler::GetFrame(FrameNumber frame_number) const
{
	// assuming not empty, a safe assumption currently

	// find the current keyframe, or the one after it
	auto next = keys.lower_bound(frame_number);
	
	// current frame is higher than any keyframe, use the last keyframe
	if (keys.end() == next)
		--next;

	auto prev = next;

	// if this is after the current frame and not the first keyframe, use the previous one
	if (frame_number < prev->first && keys.begin() != prev)
		--prev;

	const float nf = next->first - prev->first;
	if (0 == nf)
	{
		// same frame numbers, just return the first's value
		return prev->second.value;
	}
	else
	{
		// different frames, blend them together
		// this is a "Cubic Hermite spline" apparently

		// clamp frame_number
		frame_number = std::max(prev->first, std::min(next->first, frame_number));
		
		const float t = (frame_number - prev->first) / nf;

		// old curve-less code
		//return prev->second.value + (next->second.value - prev->second.value) * t;
 
		// curvy code from marcan, :p
		return
			prev->second.slope * nf * (t + powf(t, 3) - 2 * powf(t, 2)) +
			next->second.slope * nf * (powf(t, 3) - powf(t, 2)) +
			prev->second.value * (1 + (2 * powf(t, 3) - 3 * powf(t, 2))) +
			next->second.value * (-2 * powf(t, 3) + 3 * powf(t, 2));
	}
}

void StepKeyHandler::CopyFrames(const StepKeyHandler& fh, FrameNumber frame_offset)
{
	ForEach(fh.keys, [=](const std::pair<const FrameNumber, const KeyData&> frame)
	{
		keys[frame.first + frame_offset] = frame.second;
	});
}

void HermiteKeyHandler::CopyFrames(const HermiteKeyHandler& fh, FrameNumber frame_offset)
{
	ForEach(fh.keys, [=](const std::pair<const FrameNumber, const KeyData&> frame)
	{
		keys.insert(std::make_pair(frame.first + frame_offset, frame.second));
	});
}

void Animator::CopyFrames(Animator& rhs, FrameNumber frame_offset)
{
	ForEach(rhs.hermite_keys, [=](const std::pair<const KeyType&, const HermiteKeyHandler&> kf)
	{
		hermite_keys[kf.first].CopyFrames(kf.second, frame_offset);
	});

	ForEach(rhs.step_keys, [=](const std::pair<const KeyType&, const StepKeyHandler&> kf)
	{
		step_keys[kf.first].CopyFrames(kf.second, frame_offset);
	});
}

void Animator::ProcessHermiteKey(const KeyType& type, float value)
{
	std::cout << "unhandled key (" << name << "): tag: " << (int)type.tag
	<< " index: " << (int)type.index
	<< " target: " << (int)type.target
	<< '\n';
}

void Animator::ProcessStepKey(const KeyType& type, StepKeyHandler::KeyData data)
{
	std::cout << "unhandled key (" << name << "): tag: " << (int)type.tag
	<< " target: " << (int)type.target
	<< " data:" << (int)data.data1 << " " << (int)data.data2
	<< '\n';
}

}
