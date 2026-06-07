#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/sysctl.h>
#include <sys/types.h>

#include <cpuinfo/log.h>
#include <freebsd/api.h>

static int sysctl_int(const char* name) {
	int value = 0;
	size_t value_size = sizeof(value);
	if (sysctlbyname(name, &value, &value_size, NULL, 0) != 0) {
		cpuinfo_log_error("sysctlbyname(\"%s\") failed: %s", name, strerror(errno));
	} else if (value <= 0) {
		cpuinfo_log_error("sysctlbyname(\"%s\") returned invalid value %d %zu", name, value, value_size);
		value = 0;
	}
	return value;
}

static char* sysctl_str(const char* name) {
	size_t value_size = 0;
	if (sysctlbyname(name, NULL, &value_size, NULL, 0) != 0) {
		cpuinfo_log_error("sysctlbyname(\"%s\") failed: %s", name, strerror(errno));
		return NULL;
	} else if (value_size <= 0) {
		cpuinfo_log_error("sysctlbyname(\"%s\") returned invalid value size %zu", name, value_size);
		return NULL;
	}
	value_size += 1;
	char* value = calloc(value_size, 1);
	if (!value) {
		cpuinfo_log_error("calloc %zu bytes failed", value_size);
		return NULL;
	}
	if (sysctlbyname(name, value, &value_size, NULL, 0) != 0) {
		cpuinfo_log_error("sysctlbyname(\"%s\") failed: %s", name, strerror(errno));
		free(value);
		return NULL;
	}
	return value;
}

struct cpuinfo_freebsd_topology cpuinfo_freebsd_detect_topology(void) {
	struct cpuinfo_freebsd_topology topology = {
		.packages = 0,
		.cores = 0,
		.threads_per_core = 0,
		.threads = 0,
	};
	char* topology_spec = sysctl_str("kern.sched.topology_spec");
	if (!topology_spec) {
		return topology;
	}
	const char* group_tags[] = {"<group level=\"2\" cache-level=\"0\">", "<group level=\"1\" "};
	for (size_t i = 0; i < sizeof(group_tags) / sizeof(group_tags[0]); i++) {
		const char* group_tag = group_tags[i];
		char* p = strstr(topology_spec, group_tag);
		while (p) {
			topology.packages += 1;
			p++;
			p = strstr(p, group_tag);
		}
		if (topology.packages > 0) {
			break;
		}
	}

	if (topology.packages == 0) {
		cpuinfo_log_error("failed to parse topology_spec: %s", topology_spec);
		free(topology_spec);
		goto fail;
	}
	free(topology_spec);
	topology.cores = sysctl_int("kern.smp.cores");
	if (topology.cores == 0) {
		goto fail;
	}
	if (topology.cores < topology.packages) {
		cpuinfo_log_error("invalid numbers of package and core: %d %d", topology.packages, topology.cores);
		goto fail;
	}
	topology.threads_per_core = sysctl_int("kern.smp.threads_per_core");
	if (topology.threads_per_core == 0) {
		goto fail;
	}
	cpuinfo_log_debug(
		"freebsd topology: packages = %d, cores = %d, "
		"threads_per_core = %d",
		topology.packages,
		topology.cores,
		topology.threads_per_core);
	topology.threads = topology.threads_per_core * topology.cores;
	return topology;
fail:
	topology.packages = 0;
	return topology;
}
