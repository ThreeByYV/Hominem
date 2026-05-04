#include "hmnpch.h"
#include "AudioSourceSystem.h"
#include "Hominem/Core/Application.h"
#include "Hominem/Scene/Components.h"

namespace Hominem {

	void AudioSourceSystem::OnRuntimeStop(entt::registry& registry)
	{
		auto& audio = Application::Get().GetAudioSystem();
		auto view = registry.view<AudioSourceComponent>();
		for (auto e : view)
		{
			auto& src = view.get<AudioSourceComponent>(e);
			if (src.IsPlaying && src.ActiveHandle != InvalidSound)
				audio.Stop(src.ActiveHandle);
		}
	}

	void AudioSourceSystem::OnUpdate(Timestep ts, entt::registry& registry)
	{
		auto& audio = Application::Get().GetAudioSystem();
		auto view = registry.view<AudioSourceComponent>();

		for (auto e : view)
		{
			auto& src = view.get<AudioSourceComponent>(e);

			if (src.Buffer == InvalidSoundBuffer)
				continue;

			if (src.ShouldPlay && !src.IsPlaying)
			{
				src.ActiveHandle = audio.PlayEx(src.Buffer, src.Volume, src.Pitch, src.Pan, src.Loop);
				src.IsPlaying = true;
				src.PropertiesDirty = false;
			}
			else if (!src.ShouldPlay && src.IsPlaying)
			{
				audio.Stop(src.ActiveHandle);
				src.IsPlaying = false;
				src.ActiveHandle = InvalidSound;
			}
			else if (src.IsPlaying && src.PropertiesDirty)
			{
				audio.SetVolume(src.ActiveHandle, src.Volume);
				audio.SetPitch(src.ActiveHandle, src.Pitch);
				audio.SetPan(src.ActiveHandle, src.Pan);
				src.PropertiesDirty = false;
			}
		}
	}

}
